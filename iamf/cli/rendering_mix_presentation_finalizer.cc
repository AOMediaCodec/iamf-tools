/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/rendering_mix_presentation_finalizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status CollectAudioElementsInSubMix(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix) {
  audio_elements_in_sub_mix.reserve(sub_mix_audio_elements.size());
  for (const auto& audio_element : sub_mix_audio_elements) {
    auto iter = audio_elements.find(audio_element.audio_element_id);
    if (iter == audio_elements.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Audio Element with ID= ", audio_element.audio_element_id,
          " not found"));
    }
    audio_elements_in_sub_mix.push_back(&iter->second);
  }

  return absl::OkStatus();
}

absl::Status GetCommonCodecConfigPropertiesFromAudioElementIds(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    uint32_t& common_num_samples_per_frame, bool& requires_resampling) {
  absl::flat_hash_set<uint32_t> sample_rates;
  absl::flat_hash_set<uint32_t> num_samples_per_frame;
  absl::flat_hash_set<uint8_t> bit_depths;

  // Get all the bit-depths and sample_rates from each Audio Element.
  for (const auto* audio_element : audio_elements_in_sub_mix) {
    num_samples_per_frame.insert(
        audio_element->codec_config->GetNumSamplesPerFrame());
    sample_rates.insert(audio_element->codec_config->GetOutputSampleRate());
    bit_depths.insert(
        audio_element->codec_config->GetBitDepthToMeasureLoudness());
  }

  RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepth(
      sample_rates, bit_depths, common_sample_rate, common_bit_depth,
      requires_resampling));
  if (num_samples_per_frame.size() != 1) {
    return absl::InvalidArgumentError(
        "Audio elements in a submix must have the same number of samples per "
        "frame.");
  }
  common_num_samples_per_frame = *num_samples_per_frame.begin();

  return absl::OkStatus();
}

using AudioElementRenderingMetadata =
    RenderingMixPresentationFinalizer::AudioElementRenderingMetadata;

absl::Status InitializeRenderingMetadata(
    const RendererFactoryBase& renderer_factory,
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const Layout& loudness_layout, const uint32_t common_sample_rate,
    std::vector<AudioElementRenderingMetadata>& rendering_metadata_array) {
  rendering_metadata_array.resize(audio_elements_in_sub_mix.size());

  for (int i = 0; i < audio_elements_in_sub_mix.size(); i++) {
    const auto& sub_mix_audio_element = *audio_elements_in_sub_mix[i];
    auto& rendering_metadata = rendering_metadata_array[i];
    rendering_metadata.audio_element = &(sub_mix_audio_element.obu);
    rendering_metadata.codec_config = sub_mix_audio_element.codec_config;
    rendering_metadata.renderer = renderer_factory.CreateRendererForLayout(
        sub_mix_audio_element.obu.audio_substream_ids_,
        sub_mix_audio_element.substream_id_to_labels,
        rendering_metadata.audio_element->GetAudioElementType(),
        sub_mix_audio_element.obu.config_,
        sub_mix_audio_elements[i].rendering_config, loudness_layout,
        static_cast<size_t>(
            rendering_metadata.codec_config->GetNumSamplesPerFrame()));

    if (rendering_metadata.renderer == nullptr) {
      return absl::UnknownError("Unable to create renderer.");
    }

    const uint32_t output_sample_rate =
        sub_mix_audio_element.codec_config->GetOutputSampleRate();
    if (common_sample_rate != output_sample_rate) {
      // TODO(b/274689885): Convert to a common sample rate and/or bit-depth.
      return absl::UnimplementedError(
          absl::StrCat("OBUs with different sample rates not supported yet: (",
                       common_sample_rate, " != ", output_sample_rate, ")."));
    }
  }

  return absl::OkStatus();
}

absl::Status FlushUntilNonEmptyOrTimeout(
    AudioElementRendererBase& audio_element_renderer,
    std::vector<InternalSampleType>& rendered_samples) {
  static const int kMaxNumTries = 500;
  for (int i = 0; i < kMaxNumTries; i++) {
    RETURN_IF_NOT_OK(audio_element_renderer.Flush(rendered_samples));
    if (!rendered_samples.empty()) {
      // Usually samples will be ready right away. So avoid sleeping.
      return absl::OkStatus();
    }
    absl::SleepFor(absl::Milliseconds(10));
  }
  return absl::DeadlineExceededError("Timed out waiting for samples.");
}

absl::Status RenderLabeledFrameToLayout(
    const LabeledFrame& labeled_frame,
    const AudioElementRenderingMetadata& rendering_metadata,
    std::vector<InternalSampleType>& rendered_samples) {
  const auto num_time_ticks =
      rendering_metadata.renderer->RenderLabeledFrame(labeled_frame);
  if (!num_time_ticks.ok()) {
    return num_time_ticks.status();
  } else if (*num_time_ticks >
             static_cast<size_t>(
                 rendering_metadata.codec_config->GetNumSamplesPerFrame())) {
    return absl::InvalidArgumentError("Too many samples in this frame");
  } else if (*num_time_ticks == 0) {
    // This was an empty frame.
    return absl::OkStatus();
  }

  return FlushUntilNonEmptyOrTimeout(*rendering_metadata.renderer,
                                     rendered_samples);
}

absl::Status GetParameterBlockLinearMixGainsPerTick(
    uint32_t common_sample_rate, const ParameterBlockWithData& parameter_block,
    const MixGainParamDefinition& mix_gain,
    std::vector<float>& linear_mix_gain_per_tick) {
  if (mix_gain.parameter_rate_ != common_sample_rate) {
    // TODO(b/283281856): Support resampling parameter blocks.
    return absl::UnimplementedError(
        "Parameter blocks that require resampling are not supported yet.");
  }

  const int16_t default_mix_gain = mix_gain.default_mix_gain_;
  // Initialize to the default gain value.
  std::fill(linear_mix_gain_per_tick.begin(), linear_mix_gain_per_tick.end(),
            std::pow(10.0f, Q7_8ToFloat(default_mix_gain) / 20.0f));

  int32_t cur_tick = parameter_block.start_timestamp;
  // Process as many ticks as possible until all are found or the parameter
  // block ends.
  while (cur_tick < parameter_block.end_timestamp &&
         (cur_tick - parameter_block.start_timestamp) <
             linear_mix_gain_per_tick.size()) {
    RETURN_IF_NOT_OK(parameter_block.obu->GetLinearMixGain(
        cur_tick - parameter_block.start_timestamp,
        linear_mix_gain_per_tick[cur_tick - parameter_block.start_timestamp]));
    cur_tick++;
  }
  return absl::OkStatus();
}

// Fills in the output `mix_gains` with the gain in Q7.8 format to apply at each
// tick.
// TODO(b/288073842): Consider improving computational efficiency instead of
//                    searching through all parameter blocks for each frame.
// TODO(b/379961928): Remove this function once the new
//                    `GetParameterBlockLinearMixGainsPerTick()` is in use.
absl::Status GetParameterBlockLinearMixGainsPerTick(
    uint32_t common_sample_rate, int32_t start_timestamp, int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const MixGainParamDefinition& mix_gain,
    std::vector<float>& linear_mix_gain_per_tick) {
  if (mix_gain.parameter_rate_ != common_sample_rate) {
    // TODO(b/283281856): Support resampling parameter blocks.
    return absl::UnimplementedError(
        "Parameter blocks that require resampling are not supported yet.");
  }

  const auto parameter_id = mix_gain.parameter_id_;
  const int16_t default_mix_gain = mix_gain.default_mix_gain_;

  // Initialize to the default gain value.
  std::fill(linear_mix_gain_per_tick.begin(), linear_mix_gain_per_tick.end(),
            std::pow(10.0f, Q7_8ToFloat(default_mix_gain) / 20.0f));

  int32_t cur_tick = start_timestamp;

  // Find the mix gain at each tick. May terminate early if there are samples to
  // trim at the end.
  while (cur_tick < end_timestamp &&
         (cur_tick - start_timestamp) < linear_mix_gain_per_tick.size()) {
    // Find the parameter block that this tick occurs during.
    const auto parameter_block_iter = std::find_if(
        parameter_blocks.begin(), parameter_blocks.end(),
        [cur_tick, parameter_id](const auto& parameter_block) {
          return parameter_block.obu->parameter_id_ == parameter_id &&
                 parameter_block.start_timestamp <= cur_tick &&
                 cur_tick < parameter_block.end_timestamp;
        });
    if (parameter_block_iter == parameter_blocks.end()) {
      // Default mix gain will be used for this frame. Logic elsewhere validates
      // the rest of the audio frames have consistent coverage.
      break;
    }

    // Process as many ticks as possible until all are found or the parameter
    // block ends.
    while (cur_tick < end_timestamp &&
           cur_tick < parameter_block_iter->end_timestamp &&
           (cur_tick - start_timestamp) < linear_mix_gain_per_tick.size()) {
      RETURN_IF_NOT_OK(parameter_block_iter->obu->GetLinearMixGain(
          cur_tick - parameter_block_iter->start_timestamp,
          linear_mix_gain_per_tick[cur_tick - start_timestamp]));
      cur_tick++;
    }
  }

  return absl::OkStatus();
}

absl::Status GetAndApplyMixGain(  // NOLINT
    uint32_t common_sample_rate, const ParameterBlockWithData& parameter_block,
    const MixGainParamDefinition& mix_gain, int32_t num_channels,
    std::vector<InternalSampleType>& rendered_samples) {
  if (rendered_samples.size() % num_channels != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected an integer number of interlaced channels. "
        "renderered_samples.size()= ",
        rendered_samples.size(), ", num_channels= ", num_channels));
  }

  // Get the mix gain on a per tick basis from the parameter block.
  std::vector<float> linear_mix_gain_per_tick(rendered_samples.size() /
                                              num_channels);
  RETURN_IF_NOT_OK(GetParameterBlockLinearMixGainsPerTick(
      common_sample_rate, parameter_block, mix_gain, linear_mix_gain_per_tick));

  if (!linear_mix_gain_per_tick.empty()) {
    LOG_FIRST_N(INFO, 6) << " First tick in this frame has gain: "
                         << linear_mix_gain_per_tick.front();
  }

  for (int tick = 0; tick < linear_mix_gain_per_tick.size(); tick++) {
    for (int channel = 0; channel < num_channels; channel++) {
      // Apply the same mix gain to all `num_channels` associated with this
      // tick.
      rendered_samples[tick * num_channels + channel] *=
          linear_mix_gain_per_tick[tick];
    }
  }

  return absl::OkStatus();
}

// TODO(b/379961928): Remove once the new GetAndApplyMixGain is in use.
absl::Status GetAndApplyMixGain(
    uint32_t common_sample_rate, int32_t start_timestamp, int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const MixGainParamDefinition& mix_gain, int32_t num_channels,
    std::vector<float>& linear_mix_gain_per_tick,
    std::vector<InternalSampleType>& rendered_samples) {
  if (rendered_samples.size() % num_channels != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected an integer number of interlaced channels. "
        "renderered_samples.size()= ",
        rendered_samples.size(), ", num_channels= ", num_channels));
  }

  // Get the mix gain on a per tick basis from the parameter block.
  linear_mix_gain_per_tick.resize(rendered_samples.size() / num_channels, 0.0f);
  RETURN_IF_NOT_OK(GetParameterBlockLinearMixGainsPerTick(
      common_sample_rate, start_timestamp, end_timestamp, parameter_blocks,
      mix_gain, linear_mix_gain_per_tick));

  if (!linear_mix_gain_per_tick.empty()) {
    LOG_FIRST_N(INFO, 6) << " First tick in this frame has gain: "
                         << linear_mix_gain_per_tick.front();
  }

  for (int tick = 0; tick < linear_mix_gain_per_tick.size(); tick++) {
    for (int channel = 0; channel < num_channels; channel++) {
      // Apply the same mix gain to all `num_channels` associated with this
      // tick.
      rendered_samples[tick * num_channels + channel] *=
          linear_mix_gain_per_tick[tick];
    }
  }

  return absl::OkStatus();
}

absl::Status MixAudioElements(
    std::vector<std::vector<InternalSampleType>>& rendered_audio_elements,
    std::vector<InternalSampleType>& rendered_samples) {
  const size_t num_samples = rendered_audio_elements.empty()
                                 ? 0
                                 : rendered_audio_elements.front().size();
  rendered_samples.reserve(num_samples);

  for (const auto& rendered_audio_element : rendered_audio_elements) {
    if (rendered_audio_element.size() != num_samples) {
      return absl::UnknownError(
          "Expected all frames to have the same number of samples.");
    }
  }

  for (int i = 0; i < num_samples; i++) {
    InternalSampleType mixed_sample = 0;
    // Sum all audio elements for this tick.
    for (const auto& rendered_audio_element : rendered_audio_elements) {
      mixed_sample += rendered_audio_element[i];
    }
    // Push the clipped result.
    rendered_samples.push_back(mixed_sample);
  }

  return absl::OkStatus();
}

// Returns a span, which is backed by `rendered_samples`, of the ticks actually
// rendered.
absl::StatusOr<absl::Span<const std::vector<int32_t>>> RenderAllFramesForLayout(
    int32_t num_channels,
    const std::vector<SubMixAudioElement> sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain,
    const IdLabeledFrameMap& id_to_labeled_frame,
    const std::vector<AudioElementRenderingMetadata>& rendering_metadata_array,
    const int32_t start_timestamp, const int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const uint32_t common_sample_rate,
    std::vector<std::vector<int32_t>>& rendered_samples) {
  // Each audio element rendered individually with `element_mix_gain` applied.
  std::vector<std::vector<InternalSampleType>> rendered_audio_elements(
      sub_mix_audio_elements.size());
  std::vector<float> linear_mix_gain_per_tick;
  for (int i = 0; i < sub_mix_audio_elements.size(); i++) {
    const SubMixAudioElement& sub_mix_audio_element = sub_mix_audio_elements[i];
    const auto audio_element_id = sub_mix_audio_element.audio_element_id;
    const auto& rendering_metadata = rendering_metadata_array[i];

    if (id_to_labeled_frame.find(audio_element_id) !=
        id_to_labeled_frame.end()) {
      const auto& labeled_frame = id_to_labeled_frame.at(audio_element_id);
      // Render the frame to the specified `loudness_layout` and apply element
      // mix gain.
      RETURN_IF_NOT_OK(RenderLabeledFrameToLayout(
          labeled_frame, rendering_metadata, rendered_audio_elements[i]));
    }
    RETURN_IF_NOT_OK(GetAndApplyMixGain(
        common_sample_rate, start_timestamp, end_timestamp, parameter_blocks,
        sub_mix_audio_element.element_mix_gain, num_channels,
        linear_mix_gain_per_tick, rendered_audio_elements[i]));
  }

  // Mix the audio elements.
  std::vector<InternalSampleType> rendered_samples_internal;
  RETURN_IF_NOT_OK(
      MixAudioElements(rendered_audio_elements, rendered_samples_internal));

  LOG_FIRST_N(INFO, 1) << "    Applying output_mix_gain.default_mix_gain= "
                       << output_mix_gain.default_mix_gain_;

  RETURN_IF_NOT_OK(
      GetAndApplyMixGain(common_sample_rate, start_timestamp, end_timestamp,
                         parameter_blocks, output_mix_gain, num_channels,
                         linear_mix_gain_per_tick, rendered_samples_internal));

  // Convert the rendered samples to int32, clipping if needed.
  size_t num_ticks = 0;
  RETURN_IF_NOT_OK(ConvertInterleavedToTimeChannel(
      absl::MakeConstSpan(rendered_samples_internal), num_channels,
      absl::AnyInvocable<absl::Status(InternalSampleType, int32_t&) const>(
          NormalizedFloatingPointToInt32<InternalSampleType>),
      rendered_samples, num_ticks));
  return absl::MakeConstSpan(rendered_samples).first(num_ticks);
}

absl::Status ValidateUserLoudness(const LoudnessInfo& user_loudness,
                                  const uint32_t mix_presentation_id,
                                  const int sub_mix_index,
                                  const int layout_index,
                                  const LoudnessInfo& output_loudness,
                                  bool& loudness_matches_user_data) {
  const std::string mix_presentation_sub_mix_layout_index =
      absl::StrCat("Mix Presentation(ID ", mix_presentation_id, ")->sub_mixes[",
                   sub_mix_index, "]->layouts[", layout_index, "]: ");
  if (output_loudness.integrated_loudness !=
      user_loudness.integrated_loudness) {
    LOG(ERROR) << mix_presentation_sub_mix_layout_index
               << "Computed integrated loudness different from "
               << "user specification: " << output_loudness.integrated_loudness
               << " vs " << user_loudness.integrated_loudness;
    loudness_matches_user_data = false;
  }

  if (output_loudness.digital_peak != user_loudness.digital_peak) {
    LOG(ERROR) << mix_presentation_sub_mix_layout_index
               << "Computed digital peak different from "
               << "user specification: " << output_loudness.digital_peak
               << " vs " << user_loudness.digital_peak;
    loudness_matches_user_data = false;
  }

  if (output_loudness.info_type & LoudnessInfo::kTruePeak) {
    if (output_loudness.true_peak != user_loudness.true_peak) {
      LOG(ERROR) << mix_presentation_sub_mix_layout_index
                 << "Computed true peak different from "
                 << "user specification: " << output_loudness.true_peak
                 << " vs " << user_loudness.true_peak;
      loudness_matches_user_data = false;
    }
  }

  // Anchored loudness and layout extension are copied from the user input
  // and do not need to be validated.

  return absl::OkStatus();
}

// Calculates the loudness of the rendered samples. These rendered samples are
// for a specific timestamp for a given submix and layout. If
// `validate_loudness` is true, then the user provided loudness values are
// validated against the computed values.
absl::Status UpdateLoudnessInfoForLayout(
    bool validate_loudness, const LoudnessInfo& input_loudness,
    const uint32_t mix_presentation_id, const int sub_mix_index,
    const int layout_index, bool& loudness_matches_user_data,
    std::unique_ptr<LoudnessCalculatorBase> loudness_calculator,
    LoudnessInfo& output_calculated_loudness) {
  // Copy the final loudness values back to the output OBU.
  auto calculated_loudness_info = loudness_calculator->QueryLoudness();
  if (!calculated_loudness_info.ok()) {
    return calculated_loudness_info.status();
  }

  if (validate_loudness) {
    // Validate any user provided loudness values match computed values.
    RETURN_IF_NOT_OK(ValidateUserLoudness(
        input_loudness, mix_presentation_id, sub_mix_index, layout_index,
        *calculated_loudness_info, loudness_matches_user_data));
  }
  output_calculated_loudness = *calculated_loudness_info;
  return absl::OkStatus();
}

using LayoutRenderingMetadata =
    RenderingMixPresentationFinalizer::LayoutRenderingMetadata;
using SubmixRenderingMetadata =
    RenderingMixPresentationFinalizer::SubmixRenderingMetadata;

// Generates rendering metadata for all layouts within a submix. This includes
// optionally creating a sample processor and/or a loudness calculator for each
// layout.
absl::Status GenerateRenderingMetadataForLayouts(
    const RendererFactoryBase& renderer_factory,
    const LoudnessCalculatorFactoryBase* loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const DecodedUleb128 mix_presentation_id, MixPresentationSubMix& sub_mix,
    int sub_mix_index,
    std::vector<const AudioElementWithData*> audio_elements_in_sub_mix,
    uint32_t common_sample_rate, uint8_t rendering_bit_depth,
    uint32_t common_num_samples_per_frame,
    std::vector<LayoutRenderingMetadata>& output_layout_rendering_metadata) {
  output_layout_rendering_metadata.resize(sub_mix.layouts.size());
  for (int layout_index = 0; layout_index < sub_mix.layouts.size();
       layout_index++) {
    LayoutRenderingMetadata& layout_rendering_metadata =
        output_layout_rendering_metadata[layout_index];
    MixPresentationLayout& layout = sub_mix.layouts[layout_index];

    int32_t num_channels = 0;
    auto can_render_status = MixPresentationObu::GetNumChannelsFromLayout(
        layout.loudness_layout, num_channels);
    layout_rendering_metadata.num_channels = num_channels;

    can_render_status.Update(InitializeRenderingMetadata(
        renderer_factory, audio_elements_in_sub_mix, sub_mix.audio_elements,
        layout.loudness_layout, common_sample_rate,
        layout_rendering_metadata.audio_element_rendering_metadata));

    if (!can_render_status.ok()) {
      layout_rendering_metadata.can_render = false;
      continue;
    } else {
      layout_rendering_metadata.can_render = true;
    }
    if (loudness_calculator_factory != nullptr) {
      // Optionally create a loudness calculator.
      layout_rendering_metadata.loudness_calculator =
          loudness_calculator_factory->CreateLoudnessCalculator(
              layout, common_num_samples_per_frame, common_sample_rate,
              rendering_bit_depth);
    }
    // Optionally create a post-processor.
    layout_rendering_metadata.sample_processor = sample_processor_factory(
        mix_presentation_id, sub_mix_index, layout_index,
        layout.loudness_layout, num_channels, common_sample_rate,
        rendering_bit_depth, common_num_samples_per_frame);

    // Pre-allocate a buffer to store a frame's worth of rendered samples.
    layout_rendering_metadata.rendered_samples.resize(
        common_num_samples_per_frame, std::vector<int32_t>(num_channels, 0));
  }

  return absl::OkStatus();
}

// We generate one rendering metadata object for each submix. Once this
// metadata is generated, we will loop through it to render all submixes
// for a given timestamp. Within a submix, there can be many different audio
// elements and layouts that need to be rendered as well. Not all of these
// need to be rendered; only the ones that either have a wav writer or a
// loudness calculator.
absl::Status GenerateRenderingMetadataForSubmixes(
    const RendererFactoryBase& renderer_factory,
    absl::Nullable<const LoudnessCalculatorFactoryBase*>
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    MixPresentationObu& mix_presentation_obu,
    std::vector<SubmixRenderingMetadata>& output_rendering_metadata) {
  const auto mix_presentation_id = mix_presentation_obu.GetMixPresentationId();
  output_rendering_metadata.resize(mix_presentation_obu.sub_mixes_.size());
  for (int sub_mix_index = 0;
       sub_mix_index < mix_presentation_obu.sub_mixes_.size();
       ++sub_mix_index) {
    SubmixRenderingMetadata& submix_rendering_metadata =
        output_rendering_metadata[sub_mix_index];
    MixPresentationSubMix& sub_mix =
        mix_presentation_obu.sub_mixes_[sub_mix_index];

    // Pointers to audio elements in this sub mix; useful later.
    std::vector<const AudioElementWithData*> audio_elements_in_sub_mix;
    RETURN_IF_NOT_OK(CollectAudioElementsInSubMix(
        audio_elements, sub_mix.audio_elements, audio_elements_in_sub_mix));

    submix_rendering_metadata.audio_elements_in_sub_mix =
        sub_mix.audio_elements;
    submix_rendering_metadata.mix_gain =
        std::make_unique<MixGainParamDefinition>(sub_mix.output_mix_gain);

    // Data common to all audio elements and layouts.
    bool requires_resampling;
    uint32_t common_num_samples_per_frame;
    uint8_t rendering_bit_depth;
    RETURN_IF_NOT_OK(GetCommonCodecConfigPropertiesFromAudioElementIds(
        audio_elements_in_sub_mix, submix_rendering_metadata.common_sample_rate,
        rendering_bit_depth, common_num_samples_per_frame,
        requires_resampling));
    if (requires_resampling) {
      // TODO(b/274689885): Convert to a common sample rate and/or bit-depth.
      return absl::UnimplementedError(
          "This implementation does not support mixing different sample rates "
          "or bit-depths.");
    }
    RETURN_IF_NOT_OK(GenerateRenderingMetadataForLayouts(
        renderer_factory, loudness_calculator_factory, sample_processor_factory,
        mix_presentation_id, sub_mix, sub_mix_index, audio_elements_in_sub_mix,
        submix_rendering_metadata.common_sample_rate, rendering_bit_depth,
        common_num_samples_per_frame,
        submix_rendering_metadata.layout_rendering_metadata));
  }
  return absl::OkStatus();
}

absl::Status FinalizeLoudnessAndFlushPostProcessors(
    bool validate_loudness,
    std::vector<SubmixRenderingMetadata>& rendering_metadata,
    MixPresentationObu& mix_presentation_obu) {
  bool loudness_matches_user_data = true;
  int submix_index = 0;
  for (auto& submix_rendering_metadata : rendering_metadata) {
    int layout_index = 0;
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (layout_rendering_metadata.loudness_calculator != nullptr) {
        RETURN_IF_NOT_OK(UpdateLoudnessInfoForLayout(
            validate_loudness,
            mix_presentation_obu.sub_mixes_[submix_index]
                .layouts[layout_index]
                .loudness,
            mix_presentation_obu.GetMixPresentationId(), submix_index,
            layout_index, loudness_matches_user_data,
            std::move(layout_rendering_metadata.loudness_calculator),
            mix_presentation_obu.sub_mixes_[submix_index]
                .layouts[layout_index]
                .loudness));
      }
      if (layout_rendering_metadata.sample_processor != nullptr) {
        RETURN_IF_NOT_OK(layout_rendering_metadata.sample_processor->Flush());
      }
      layout_index++;
    }
    submix_index++;
  }
  if (!loudness_matches_user_data) {
    return absl::InvalidArgumentError("Loudness does not match user data.");
  }
  return absl::OkStatus();
}

bool CanRenderAnyLayout(
    const std::vector<SubmixRenderingMetadata>& rendering_metadata) {
  for (auto& submix_rendering_metadata : rendering_metadata) {
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (layout_rendering_metadata.can_render) {
        return true;
      }
    }
  }
  return false;
}

// Renders all submixes, layouts, and audio elements for a temporal unit. It
// then optionally writes the rendered samples to a wav file and/or calculates
// the loudness of the rendered samples.
absl::Status RenderWriteAndCalculateLoudnessForTemporalUnit(
    const IdLabeledFrameMap& id_to_labeled_frame, const int32_t start_timestamp,
    const int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    std::vector<SubmixRenderingMetadata>& rendering_metadata) {
  for (auto& submix_rendering_metadata : rendering_metadata) {
    for (auto& layout_rendering_metadata :
         submix_rendering_metadata.layout_rendering_metadata) {
      if (!layout_rendering_metadata.can_render) {
        continue;
      }
      if (submix_rendering_metadata.mix_gain == nullptr) {
        return absl::InvalidArgumentError("Submix mix gain is null");
      }

      const auto rendered_span = RenderAllFramesForLayout(
          layout_rendering_metadata.num_channels,
          submix_rendering_metadata.audio_elements_in_sub_mix,
          *submix_rendering_metadata.mix_gain, id_to_labeled_frame,
          layout_rendering_metadata.audio_element_rendering_metadata,
          start_timestamp, end_timestamp, parameter_blocks,
          submix_rendering_metadata.common_sample_rate,
          layout_rendering_metadata.rendered_samples);
      if (!rendered_span.ok()) {
        return rendered_span.status();
      }
      // Calculate loudness based on the original rendered samples; we do not
      // know what post-processing the end user will have.
      if (layout_rendering_metadata.loudness_calculator != nullptr) {
        RETURN_IF_NOT_OK(layout_rendering_metadata.loudness_calculator
                             ->AccumulateLoudnessForSamples(*rendered_span));
      }

      // Perform any post-processing.
      if (layout_rendering_metadata.sample_processor != nullptr) {
        RETURN_IF_NOT_OK(layout_rendering_metadata.sample_processor->PushFrame(
            *rendered_span));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<RenderingMixPresentationFinalizer>
RenderingMixPresentationFinalizer::Create(
    absl::Nullable<const RendererFactoryBase*> renderer_factory,
    absl::Nullable<const LoudnessCalculatorFactoryBase*>
        loudness_calculator_factory,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const SampleProcessorFactory& sample_processor_factory,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  if (renderer_factory == nullptr) {
    LOG(INFO) << "Rendering is safely disabled.";
    return RenderingMixPresentationFinalizer(
        std::vector<MixPresentationRenderingMetadata>());
  }
  if (loudness_calculator_factory == nullptr) {
    LOG(INFO) << "Loudness calculator factory is null so loudness will not be "
                 "calculated.";
  }
  std::vector<MixPresentationRenderingMetadata> rendering_metadata;
  rendering_metadata.reserve(mix_presentation_obus.size());
  for (auto& mix_presentation_obu : mix_presentation_obus) {
    std::vector<SubmixRenderingMetadata> sub_mix_rendering_metadata;
    RETURN_IF_NOT_OK(GenerateRenderingMetadataForSubmixes(
        *renderer_factory, loudness_calculator_factory,
        sample_processor_factory, audio_elements, mix_presentation_obu,
        sub_mix_rendering_metadata));

    rendering_metadata.push_back(MixPresentationRenderingMetadata{
        .mix_presentation_id = mix_presentation_obu.GetMixPresentationId(),
        .submix_rendering_metadata = std::move(sub_mix_rendering_metadata),
    });
  }

  return RenderingMixPresentationFinalizer(std::move(rendering_metadata));
}

absl::Status RenderingMixPresentationFinalizer::PushTemporalUnit(
    const IdLabeledFrameMap& id_to_labeled_frame, const int32_t start_timestamp,
    const int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks) {
  for (auto& mix_presentation_rendering_metadata : rendering_metadata_) {
    if (!CanRenderAnyLayout(
            mix_presentation_rendering_metadata.submix_rendering_metadata)) {
      LOG(INFO) << "No layouts can be rendered";
      continue;
    }
    RETURN_IF_NOT_OK(RenderWriteAndCalculateLoudnessForTemporalUnit(
        id_to_labeled_frame, start_timestamp, end_timestamp, parameter_blocks,
        mix_presentation_rendering_metadata.submix_rendering_metadata));
  }
  return absl::OkStatus();
}

absl::Status RenderingMixPresentationFinalizer::Finalize(
    bool validate_loudness,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  if (rendering_is_disabled_) {
    LOG(INFO) << "Renderer is disabled; so rendering is safely aborted.";
    return absl::OkStatus();
  }
  if (rendering_metadata_.size() != mix_presentation_obus.size()) {
    return absl::InvalidArgumentError(
        "Size mismatch between rendering metadata and mix presentation OBUs.");
  }
  int i = 0;
  for (auto& mix_presentation_obu : mix_presentation_obus) {
    if (rendering_metadata_[i].mix_presentation_id !=
        mix_presentation_obu.GetMixPresentationId()) {
      return absl::InvalidArgumentError(
          "Mix presentation ID mismatch between rendering metadata and mix "
          "presentation OBUs.");
    }
    RETURN_IF_NOT_OK(FinalizeLoudnessAndFlushPostProcessors(
        validate_loudness, rendering_metadata_[i].submix_rendering_metadata,
        mix_presentation_obu));
    i++;
  }
  // Clearing rendering metadata closes all renderers,  and loudness
  // calculators.
  rendering_metadata_.clear();
  return absl::OkStatus();
}

}  // namespace iamf_tools
