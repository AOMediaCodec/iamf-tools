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
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
#include "iamf/cli/wav_writer.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
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

absl::Status GetCommonSampleRateAndBitDepthFromAudioElementIds(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  absl::flat_hash_set<uint32_t> sample_rates;
  absl::flat_hash_set<uint8_t> bit_depths;

  // Get all the bit-depths and sample_rates from each Audio Element.
  for (const auto* audio_element : audio_elements_in_sub_mix) {
    sample_rates.insert(audio_element->codec_config->GetOutputSampleRate());
    bit_depths.insert(
        audio_element->codec_config->GetBitDepthToMeasureLoudness());
  }

  RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepth(
      sample_rates, bit_depths, common_sample_rate, common_bit_depth,
      requires_resampling));
  return absl::OkStatus();
}

// Common metadata for rendering an audio element and independent of
// each frame.
struct AudioElementRenderingMetadata {
  std::unique_ptr<AudioElementRendererBase> renderer;

  // Pointers to the audio element and the associated codec config. They
  // contain useful information for rendering.
  const AudioElementObu* audio_element;
  const CodecConfigObu* codec_config;
};

absl::Status InitializeRenderingMetadata(
    const RendererFactoryBase& renderer_factory,
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const Layout& loudness_layout, const uint32_t common_sample_rate,
    const uint8_t common_bit_depth,
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
        sub_mix_audio_elements[i].rendering_config, loudness_layout);

    if (rendering_metadata.renderer == nullptr) {
      return absl::UnknownError("Unable to create renderer.");
    }

    const uint32_t output_sample_rate =
        sub_mix_audio_element.codec_config->GetOutputSampleRate();
    const uint8_t output_bit_depth =
        sub_mix_audio_element.codec_config->GetBitDepthToMeasureLoudness();
    if (common_sample_rate != output_sample_rate ||
        common_bit_depth != output_bit_depth) {
      // TODO(b/274689885): Convert to a common sample rate and/or bit-depth.
      return absl::UnimplementedError(absl::StrCat(
          "OBUs with different sample rates or bit-depths not supported yet: (",
          common_sample_rate, " != ", output_sample_rate, " or ",
          common_bit_depth, " != ", output_bit_depth, ")."));
    }
  }

  return absl::OkStatus();
}

absl::Status SleepUntilFinalizedOrTimeout(
    const AudioElementRendererBase& audio_element_renderer) {
  const int kMaxNumTries = 500;
  for (int i = 0; i < kMaxNumTries; i++) {
    if (audio_element_renderer.IsFinalized()) {
      // Usually it will be finalized right away. So avoid sleeping.
      return absl::OkStatus();
    }
    absl::SleepFor(absl::Milliseconds(10));
  }
  return absl::DeadlineExceededError("Timed out waiting to finalize.");
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

// Fills in the output `mix_gains` with the gain in Q7.8 format to apply at each
// tick.
// TODO(b/288073842): Consider improving computational efficiency instead of
//                    searching through all parameter blocks for each frame.
absl::Status GetParameterBlockMixGainsPerTick(
    uint32_t common_sample_rate, int32_t start_timestamp, int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const MixGainParamDefinition& mix_gain,
    std::vector<int16_t>& mix_gain_per_tick) {
  if (mix_gain.parameter_rate_ != common_sample_rate) {
    // TODO(b/283281856): Support resampling parameter blocks.
    return absl::UnimplementedError(
        "Parameter blocks that require resampling are not supported yet.");
  }

  const auto parameter_id = mix_gain.parameter_id_;
  const int16_t default_mix_gain = mix_gain.default_mix_gain_;

  // Initialize to the default gain value.
  std::fill(mix_gain_per_tick.begin(), mix_gain_per_tick.end(),
            default_mix_gain);

  int32_t cur_tick = start_timestamp;

  // Find the mix gain at each tick. May terminate early if there are samples to
  // trim at the end.
  while (cur_tick < end_timestamp &&
         (cur_tick - start_timestamp) < mix_gain_per_tick.size()) {
    // Find the parameter block that this tick occurs during.
    const auto parameter_block = std::find_if(
        parameter_blocks.begin(), parameter_blocks.end(),
        [cur_tick, parameter_id](const auto& parameter_block) {
          return parameter_block.obu->parameter_id_ == parameter_id &&
                 parameter_block.start_timestamp <= cur_tick &&
                 cur_tick < parameter_block.end_timestamp;
        });
    if (parameter_block == parameter_blocks.end()) {
      // Default mix gain will be used for this frame. Logic elsewhere validates
      // the rest of the audio frames have consistent coverage.
      break;
    }

    // Process as many ticks as possible until all are found or the parameter
    // block ends.
    while (cur_tick < end_timestamp &&
           cur_tick < parameter_block->end_timestamp &&
           (cur_tick - start_timestamp) < mix_gain_per_tick.size()) {
      RETURN_IF_NOT_OK(parameter_block->obu->GetMixGain(
          cur_tick - parameter_block->start_timestamp,
          mix_gain_per_tick[cur_tick - start_timestamp]));
      cur_tick++;
    }
  }

  return absl::OkStatus();
}

// Applies the `mix_gain` in Q7.8 format to the output sample.
absl::Status ApplyMixGain(int16_t mix_gain, InternalSampleType& sample) {
  const double mix_gain_db = Q7_8ToFloat(mix_gain);
  sample *= std::pow(10, mix_gain_db / 20);
  return absl::OkStatus();
}

absl::Status GetAndApplyMixGain(
    uint32_t common_sample_rate, int32_t start_timestamp, int32_t end_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const MixGainParamDefinition& mix_gain, int32_t num_channels,
    std::vector<InternalSampleType>& rendered_samples) {
  if (rendered_samples.size() % num_channels != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected an integer number of interlaced channels. "
        "renderered_samples.size()= ",
        rendered_samples.size(), ", num_channels= ", num_channels));
  }

  // Get the mix gain on a per tick basis from the parameter block.
  std::vector<int16_t> mix_gain_per_tick(rendered_samples.size() /
                                         num_channels);
  RETURN_IF_NOT_OK(GetParameterBlockMixGainsPerTick(
      common_sample_rate, start_timestamp, end_timestamp, parameter_blocks,
      mix_gain, mix_gain_per_tick));

  if (!mix_gain_per_tick.empty()) {
    LOG_FIRST_N(INFO, 6) << " First tick in this frame has gain: "
                         << mix_gain_per_tick.front();
  }

  for (int tick = 0; tick < mix_gain_per_tick.size(); tick++) {
    for (int channel = 0; channel < num_channels; channel++) {
      // Apply the same mix gain to all `num_channels` associated with this
      // tick.
      RETURN_IF_NOT_OK(
          ApplyMixGain(mix_gain_per_tick[tick],
                       rendered_samples[tick * num_channels + channel]));
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

absl::Status RenderNextFrameForLayout(
    int32_t num_channels,
    const std::vector<SubMixAudioElement> sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain,
    const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
    const std::vector<AudioElementRenderingMetadata>& rendering_metadata_array,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const uint32_t common_sample_rate, int32_t& start_timestamp,
    std::vector<int32_t>& rendered_samples) {
  LOG_FIRST_N(INFO, 1) << "Rendering start_timestamp= " << start_timestamp;

  rendered_samples.clear();
  // TODO(b/273464424): To support enhanced profile remove assumption that
  //                    all audio frames are aligned and have the same
  //                    duration.
  int32_t end_timestamp = start_timestamp;

  // Each audio element rendered individually with `element_mix_gain` applied.
  std::vector<std::vector<InternalSampleType>> rendered_audio_elements(
      sub_mix_audio_elements.size());
  for (int i = 0; i < sub_mix_audio_elements.size(); i++) {
    const SubMixAudioElement& sub_mix_audio_element = sub_mix_audio_elements[i];
    const auto audio_element_id = sub_mix_audio_element.audio_element_id;
    const auto& time_to_labeled_frame =
        id_to_time_to_labeled_frame.at(audio_element_id);
    const auto& rendering_metadata = rendering_metadata_array[i];

    if (time_to_labeled_frame.find(start_timestamp) !=
        time_to_labeled_frame.end()) {
      const auto& labeled_frame = time_to_labeled_frame.at(start_timestamp);
      end_timestamp = labeled_frame.end_timestamp;

      // Render the frame to the specified `loudness_layout` and apply element
      // mix gain.
      RETURN_IF_NOT_OK(RenderLabeledFrameToLayout(
          labeled_frame, rendering_metadata, rendered_audio_elements[i]));

    } else {
      // This can happen when reaching the end of the stream. Flush and
      // calculate the final gains.
      LOG(INFO) << "Rendering END";
      RETURN_IF_NOT_OK(rendering_metadata.renderer->Finalize());
      RETURN_IF_NOT_OK(
          SleepUntilFinalizedOrTimeout(*rendering_metadata.renderer));

      RETURN_IF_NOT_OK(
          rendering_metadata.renderer->Flush(rendered_audio_elements[i]));
    }

    RETURN_IF_NOT_OK(GetAndApplyMixGain(
        common_sample_rate, start_timestamp, end_timestamp, parameter_blocks,
        sub_mix_audio_element.element_mix_gain, num_channels,
        rendered_audio_elements[i]));
  }  // End of for (int i = 0; i < num_audio_elements; i++)

  // Mix the audio elements.
  std::vector<InternalSampleType> rendered_samples_internal;
  RETURN_IF_NOT_OK(
      MixAudioElements(rendered_audio_elements, rendered_samples_internal));

  LOG_FIRST_N(INFO, 1) << "    Applying output_mix_gain.default_mix_gain= "
                       << output_mix_gain.default_mix_gain_;

  RETURN_IF_NOT_OK(GetAndApplyMixGain(
      common_sample_rate, start_timestamp, end_timestamp, parameter_blocks,
      output_mix_gain, num_channels, rendered_samples_internal));

  // Convert the rendered samples to int32, clipping if needed.
  rendered_samples.reserve(rendered_samples_internal.size());
  for (const InternalSampleType sample : rendered_samples_internal) {
    int32_t sample_int32;
    RETURN_IF_NOT_OK(ClipDoubleToInt32(sample, sample_int32));
    rendered_samples.push_back(sample_int32);
  }

  // Set the output argument to the next timestamp in the series.
  start_timestamp = end_timestamp;

  return absl::OkStatus();
}

// Convert the samples from left-justified 32 bit to the little endian PCM with
// the expected bit-depth. Write the native format write to the wav file.
absl::Status WriteRenderedSamples(const std::vector<int32_t>& rendered_samples,
                                  const uint8_t common_bit_depth,
                                  WavWriter& wav_writer) {
  std::vector<uint8_t> native_samples(
      rendered_samples.size() * common_bit_depth / 8, 0);
  int write_position = 0;
  for (const int32_t sample : rendered_samples) {
    // `WritePcmSample` requires the input sample to be in the upper
    // bits of the first argument.
    RETURN_IF_NOT_OK(WritePcmSample(sample, common_bit_depth,
                                    /*big_endian=*/false, native_samples.data(),
                                    write_position));
  }
  wav_writer.WriteSamples(native_samples);

  return absl::OkStatus();
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

// Contains rendering metadata for all audio elements in a given layout.
struct LayoutRenderingMetadata {
  // Controlled by the WavWriterFactory; may be nullptr if the user does not
  // want a wav file written for this layout.
  std::unique_ptr<WavWriter> wav_writer;
  // Controlled by the LoudnessCalculatorFactory; may be nullptr if the user
  // does not want loudness calculated for this layout.
  std::unique_ptr<LoudnessCalculatorBase> loudness_calculator;
  std::vector<AudioElementRenderingMetadata> audio_element_rendering_metadata;
};

// We need to store rendering metadata for each submix, layout, and audio
// element. This metadata will then be used to render the audio frames at each
// timestamp. Some metadata is common to all audio elements and all layouts
// within a submix. We also want to optionally support writing to a wav file
// and/or calculating loudness based on the rendered output.
struct SubmixRenderingMetadata {
  uint32_t common_sample_rate;
  uint8_t common_bit_depth;
  // This vector will contain one LayoutRenderingMetadata per layout in the
  // submix.
  std::vector<LayoutRenderingMetadata> layout_rendering_metadata;
};

absl::Status GenerateRenderingMetadataForLayouts(
    uint32_t output_bit_depth,
    std::vector<LayoutRenderingMetadata>& output_layout_rendering_metadata) {
  return absl::UnimplementedError("Not implemented yet.");
}

// We generate one rendering metadata object for each submix. Once this
// metadata is generated, we will loop through it to render all submixes
// for a given timestamp. Within a submix, there can be many different audio
// elements and layouts that need to be rendered as well. Not all of these
// need to be rendered; only the ones that either have a wav writer or a
// loudness calculator.
absl::Status GenerateRenderingMetadataForSubmixes(  // NOLINT
    const RendererFactoryBase& renderer_factory,
    const LoudnessCalculatorFactoryBase* loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::WavWriterFactory&
        wav_writer_factory,
    const std::filesystem::path& file_path_prefix,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::optional<uint32_t> output_wav_file_bit_depth_override,
    MixPresentationObu& mix_presentation_obu,
    std::vector<SubmixRenderingMetadata>& output_rendering_metadata) {
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

    // Data common to all audio elements and layouts.
    bool requires_resampling;
    RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepthFromAudioElementIds(
        audio_elements_in_sub_mix, submix_rendering_metadata.common_sample_rate,
        submix_rendering_metadata.common_bit_depth, requires_resampling));
    if (requires_resampling) {
      // TODO(b/274689885): Convert to a common sample rate and/or bit-depth.
      return absl::UnimplementedError(
          "This implementation does not support mixing different sample rates "
          "or bit-depths.");
    }
    const auto output_wav_file_bit_depth =
        output_wav_file_bit_depth_override.has_value()
            ? *output_wav_file_bit_depth_override
            : submix_rendering_metadata.common_bit_depth;
    std::vector<LayoutRenderingMetadata>& layout_rendering_metadata =
        submix_rendering_metadata.layout_rendering_metadata;
    RETURN_IF_NOT_OK(GenerateRenderingMetadataForLayouts(
        output_wav_file_bit_depth, layout_rendering_metadata));
  }
  return absl::OkStatus();
}

// TODO(b/379727145): This function should be split up into smaller functions.
// It is rendering and then optionally writing to a wav file and/or calculating
// loudness based on the rendered output. We should split up the rendering part
// from the loudness calculation part ideally and rename accordingly. It also
// does this for each submix, each layout within a submix, and each audio
// element at every timestamp. We want to change it such that it does similar
// behavior for only one timestamp at a time.
absl::Status FillLoudnessInfo(
    bool validate_loudness, const RendererFactoryBase& renderer_factory,
    const LoudnessCalculatorFactoryBase* loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::WavWriterFactory&
        wav_writer_factory,
    const std::filesystem::path& file_path_prefix,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
    const int32_t min_start_time, const int32_t max_end_time,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::optional<uint32_t> output_wav_file_bit_depth_override,
    MixPresentationObu& mix_presentation_obu) {
  bool loudness_matches_user_data = true;
  const auto mix_presentation_id = mix_presentation_obu.GetMixPresentationId();

  for (int sub_mix_index = 0;
       sub_mix_index < mix_presentation_obu.sub_mixes_.size();
       ++sub_mix_index) {
    MixPresentationSubMix& sub_mix =
        mix_presentation_obu.sub_mixes_[sub_mix_index];

    // Pointers to audio elements in this sub mix; useful later.
    std::vector<const AudioElementWithData*> audio_elements_in_sub_mix;
    RETURN_IF_NOT_OK(CollectAudioElementsInSubMix(
        audio_elements, sub_mix.audio_elements, audio_elements_in_sub_mix));

    // Data common to all audio elements and layouts.
    uint32_t common_sample_rate;
    uint8_t common_bit_depth;
    bool requires_resampling;
    RETURN_IF_NOT_OK(GetCommonSampleRateAndBitDepthFromAudioElementIds(
        audio_elements_in_sub_mix, common_sample_rate, common_bit_depth,
        requires_resampling));
    if (requires_resampling) {
      // TODO(b/274689885): Convert to a common sample rate and/or bit-depth.
      return absl::UnknownError(
          "This implementation does not support mixing different sample rates "
          "or bit-depths.");
    }
    const auto output_wav_file_bit_depth =
        output_wav_file_bit_depth_override.has_value()
            ? *output_wav_file_bit_depth_override
            : common_bit_depth;

    // Render all audio elements to all layouts.
    for (int layout_index = 0; layout_index < sub_mix.layouts.size();
         layout_index++) {
      MixPresentationLayout& layout = sub_mix.layouts[layout_index];

      int32_t num_channels = 0;
      auto can_render_status = MixPresentationObu::GetNumChannelsFromLayout(
          layout.loudness_layout, num_channels);

      int32_t start_timestamp = min_start_time;

      std::vector<AudioElementRenderingMetadata> rendering_metadata_array;
      can_render_status.Update(InitializeRenderingMetadata(
          renderer_factory, audio_elements_in_sub_mix, sub_mix.audio_elements,
          layout.loudness_layout, common_sample_rate, common_bit_depth,
          rendering_metadata_array));

      if (!can_render_status.ok()) {
        LOG(WARNING) << "Rendering is not supported yet for this layout: "
                     << can_render_status
                     << ". Skipping rendering and loudness calculation.";
        continue;
      }
      std::unique_ptr<LoudnessCalculatorBase> loudness_calculator = nullptr;
      if (loudness_calculator_factory != nullptr) {
        loudness_calculator =
            loudness_calculator_factory->CreateLoudnessCalculator(
                layout, common_sample_rate, common_bit_depth);
      }

      // Rendering is supported. Render the samples to measure loudness. Try
      // to create a wav writer, but it is OK if the user disabled it in this
      // context.
      auto wav_writer = wav_writer_factory(
          mix_presentation_id, sub_mix_index, layout_index,
          layout.loudness_layout, file_path_prefix, num_channels,
          common_sample_rate, output_wav_file_bit_depth);

      do {
        std::vector<int32_t> rendered_samples;
        RETURN_IF_NOT_OK(RenderNextFrameForLayout(
            num_channels, sub_mix.audio_elements, sub_mix.output_mix_gain,
            id_to_time_to_labeled_frame, rendering_metadata_array,
            parameter_blocks, common_sample_rate, start_timestamp,
            rendered_samples));

        if (wav_writer != nullptr) {
          RETURN_IF_NOT_OK(WriteRenderedSamples(
              rendered_samples, output_wav_file_bit_depth, *wav_writer));
        }

        if (loudness_calculator != nullptr) {
          RETURN_IF_NOT_OK(loudness_calculator->AccumulateLoudnessForSamples(
              rendered_samples));
        }
      } while (start_timestamp != max_end_time);
      if (loudness_calculator == nullptr) {
        // Rendering is done, and loudness is not being calculated. Ok.
        continue;
      }

      // Copy the final loudness values back to the output OBU.
      auto calculated_loudness_info = loudness_calculator->QueryLoudness();
      if (!calculated_loudness_info.ok()) {
        return calculated_loudness_info.status();
      }

      if (validate_loudness) {
        // Validate any user provided loudness values match computed values.
        RETURN_IF_NOT_OK(ValidateUserLoudness(
            layout.loudness, mix_presentation_id, sub_mix_index, layout_index,
            *calculated_loudness_info, loudness_matches_user_data));
      }
      layout.loudness = *calculated_loudness_info;
    }  // End loop over all layouts.
  }  // End loop over all submixes.

  if (!loudness_matches_user_data) {
    return absl::InvalidArgumentError("Loudness does not match user data.");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status RenderingMixPresentationFinalizer::Finalize(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const WavWriterFactory& wav_writer_factory,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  if (renderer_factory_ == nullptr) {
    // Ok. When rendering is disabled, there is nothing to finalize.
    for (const auto& mix_presentation_obu : mix_presentation_obus) {
      mix_presentation_obu.PrintObu();
    }
    return absl::OkStatus();
  }
  // Find the minimum start timestamp and maximum end timestamp.
  int32_t min_start_time = INT32_MAX;
  int32_t max_end_time = INT32_MIN;
  for (const auto& [unused_id, time_to_labeled_frame] :
       id_to_time_to_labeled_frame) {
    // `time_to_labeled_frame` is already sorted by the starting timestamps,
    // so we just have to probe the first and the last frames.
    min_start_time =
        std::min(min_start_time, time_to_labeled_frame.begin()->first);
    max_end_time = std::max(
        max_end_time, time_to_labeled_frame.rbegin()->second.end_timestamp);
  }

  // Finalize all OBUs by calculating the loudness.
  int i = 0;
  for (auto& mix_presentation_obu : mix_presentation_obus) {
    RETURN_IF_NOT_OK(FillLoudnessInfo(
        validate_loudness_, *renderer_factory_,
        loudness_calculator_factory_.get(), wav_writer_factory,
        file_path_prefix_, audio_elements, id_to_time_to_labeled_frame,
        min_start_time, max_end_time, parameter_blocks,
        output_wav_file_bit_depth_override_, mix_presentation_obu));
    i++;
  }

  // Examine finalized Mix Presentation OBUs.
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    mix_presentation_obu.PrintObu();
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
