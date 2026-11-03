/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/obu_processor.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/obu_processor_utils.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/cli/temporal_unit_processor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// Validates that all Mix Presentation OBUs use the same Codec Config OBU in
// their first sub-mix.
absl::Status ValidateFirstSubMixesUseSameCodecConfig(
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus) {
  std::optional<DecodedUleb128> first_sub_mix_codec_config_id;
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    if (mix_presentation_obu.sub_mixes_.empty()) {
      continue;
    }
    for (const auto& sub_mix_audio_element :
         mix_presentation_obu.sub_mixes_.front().audio_elements) {
      const auto audio_element_iter =
          audio_elements.find(sub_mix_audio_element.audio_element_id);
      if (audio_element_iter == audio_elements.end() ||
          audio_element_iter->second.codec_config == nullptr) {
        continue;
      }
      const DecodedUleb128 codec_config_id =
          audio_element_iter->second.codec_config->GetCodecConfigId();
      if (!first_sub_mix_codec_config_id.has_value()) {
        first_sub_mix_codec_config_id = codec_config_id;
      } else if (*first_sub_mix_codec_config_id != codec_config_id) {
        return absl::InvalidArgumentError(
            "Mix presentations do not use the same Codec Config OBU in their "
            "first sub-mix.");
      }
    }
  }
  return absl::OkStatus();
}

// Returns a list of pointers to the supported mix presentations. Empty if none
// are supported.
std::list<MixPresentationObu*> GetSupportedMixPresentations(
    ProfileVersion primary_profile,
    const absl::flat_hash_set<ProfileVersion> requested_profiles,
    const DescriptorObus::AudioElementsById& audio_elements,
    DescriptorObus::MixPresentationObus& mix_presentation_obus) {
  if (const auto status = ValidateFirstSubMixesUseSameCodecConfig(
          audio_elements, mix_presentation_obus);
      !status.ok()) {
    ABSL_LOG(INFO) << status.message();
    return {};
  }

  if (const auto status =
          ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
              primary_profile, audio_elements, mix_presentation_obus);
      !status.ok()) {
    ABSL_LOG(INFO) << status.message();
    return {};
  }

  // Find a mix presentation and layout that agrees with the requested profiles.
  std::list<MixPresentationObu*> supported_mix_presentations;
  std::string cumulative_error_message;
  for (auto iter = mix_presentation_obus.begin();
       iter != mix_presentation_obus.end(); ++iter) {
    auto profiles = requested_profiles;
    const auto status = ProfileFilter::FilterProfilesForMixPresentation(
        audio_elements, *iter, profiles);
    if (status.ok()) {
      supported_mix_presentations.push_back(&*iter);
      if (iter == mix_presentation_obus.begin() &&
          primary_profile == ProfileVersion::kIamfBaseAdvancedProfile &&
          !requested_profiles.contains(ProfileVersion::kIamfAdvanced1Profile) &&
          !requested_profiles.contains(ProfileVersion::kIamfAdvanced2Profile) &&
          ContainsObjectBasedAudioElement(audio_elements, *iter)) {
        // Per IAMF Spec §4.4, when primary_profile is Base-Advanced, the
        // decoder does not support Advanced-1/Advanced-2, and the first Mix
        // Presentation OBU contains an object-based Audio Element, we must use
        // the first Mix Presentation OBU.
        break;
      }
    }
    absl::StrAppend(&cumulative_error_message, status.message(), "\n");
  }
  ABSL_LOG(INFO) << "Filtered mix presentations: " << cumulative_error_message
                 << ". Number of supported mix presentations: "
                 << supported_mix_presentations.size();
  return supported_mix_presentations;
}

void GetSampleRateAndFrameSize(
    const DescriptorObus::CodecConfigsById& output_codec_config_obus,
    std::optional<uint32_t>& output_sample_rate,
    std::optional<uint32_t>& output_frame_size) {
  if (output_codec_config_obus.empty()) {
    ABSL_LOG(WARNING) << "No codec config OBUs found.";
    return;
  }
  const auto& first_codec_config_obu = output_codec_config_obus.begin()->second;
  const uint32_t common_sample_rate =
      first_codec_config_obu.GetOutputSampleRate();
  const uint32_t common_frame_size =
      first_codec_config_obu.GetNumSamplesPerFrame();
  for (const auto& [unused_id, codec_config_obu] : output_codec_config_obus) {
    if (codec_config_obu.GetOutputSampleRate() != common_sample_rate ||
        codec_config_obu.GetNumSamplesPerFrame() != common_frame_size) {
      ABSL_LOG(WARNING)
          << "Codec config OBUs do not have a common sample rate and frame "
             "size.";
      return;
    }
  }
  output_sample_rate = common_sample_rate;
  output_frame_size = common_frame_size;
}

}  // namespace

absl::Status ObuProcessor::InitializeInternal(bool is_exhaustive_and_exact,
                                              bool& output_insufficient_data) {
  // Process the descriptor OBUs.
  ABSL_LOG(INFO) << "Starting Descriptor OBU processing";
  absl::StatusOr<DescriptorObus> parsed_obus =
      DescriptorObuParser::ProcessDescriptorObus(
          is_exhaustive_and_exact, *read_bit_buffer_, output_insufficient_data);
  if (!parsed_obus.ok()) {
    return parsed_obus.status();
  }
  descriptors_ = *std::move(parsed_obus);

  ABSL_LOG(INFO) << "Processed Descriptor OBUs";
  GetSampleRateAndFrameSize(descriptors_.codec_config_obus, output_sample_rate_,
                            output_frame_size_);
  auto temporal_unit_processor = TemporalUnitProcessor::Create(
      descriptors_.audio_elements, descriptors_.mix_presentation_obus,
      read_bit_buffer_);
  if (!temporal_unit_processor.ok()) {
    return temporal_unit_processor.status();
  }
  temporal_unit_processor_.emplace(*std::move(temporal_unit_processor));
  return absl::OkStatus();
}

const IASequenceHeaderObu& ObuProcessor::GetIaSequenceHeaderView() const {
  return descriptors_.ia_sequence_header;
}

const DescriptorObus::CodecConfigsById& ObuProcessor::GetCodecConfigsByIdView()
    const {
  return descriptors_.codec_config_obus;
}

const DescriptorObus::AudioElementsById& ObuProcessor::GetAudioElementsView()
    const {
  return descriptors_.audio_elements;
}

const DescriptorObus::MixPresentationObus&
ObuProcessor::GetMixPresentationObusView() const {
  return descriptors_.mix_presentation_obus;
}

const DescriptorObus::MetadataObus& ObuProcessor::GetMetadataObusView() const {
  return descriptors_.metadata_obus;
}

std::unique_ptr<ObuProcessor> absl_nullable ObuProcessor::Create(
    bool is_exhaustive_and_exact, ReadBitBuffer* absl_nonnull read_bit_buffer,
    bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

std::unique_ptr<ObuProcessor> absl_nullable ObuProcessor::CreateForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const std::optional<uint32_t>& desired_mix_presentation_id,
    const std::optional<Layout>& desired_layout, bool is_exhaustive_and_exact,
    ReadBitBuffer* absl_nonnull read_bit_buffer,
    TrimmingSettings trimming_settings, bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  if (const auto status = obu_processor->InitializeForRendering(
          desired_profile_versions, desired_mix_presentation_id, desired_layout,
          trimming_settings);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputSampleRate() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_sample_rate_,
                       "Output sample rate, was this a trivial IA Sequence?"));
  return *output_sample_rate_;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputFrameSize() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_frame_size_,
                       "Output frame size, was this a trivial IA Sequence?"));
  return *output_frame_size_;
}

absl::StatusOr<DecodedUleb128> ObuProcessor::GetOutputMixPresentationId()
    const {
  if (rendering_models_.has_value()) {
    return decoding_layout_info_.mix_presentation_id;
  }
  return absl::FailedPreconditionError("Not initialized for rendering.");
}

absl::StatusOr<Layout> ObuProcessor::GetOutputLayout() const {
  if (rendering_models_.has_value()) {
    return decoding_layout_info_.layout;
  }
  return absl::FailedPreconditionError("Not initialized for rendering.");
}

absl::Status ObuProcessor::InitializeForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const std::optional<uint32_t>& desired_mix_presentation_id,
    const std::optional<Layout>& desired_layout,
    TrimmingSettings trimming_settings) {
  if (descriptors_.mix_presentation_obus.empty()) {
    return absl::InvalidArgumentError("No mix presentation OBUs found.");
  }
  if (descriptors_.audio_elements.empty()) {
    return absl::InvalidArgumentError("No audio element OBUs found.");
  }
  RETURN_IF_NOT_OK(ProfileFilter::ValidateAudioElementsForAdditionalProfile(
      descriptors_.ia_sequence_header.GetAdditionalProfile(),
      descriptors_.audio_elements));

  const std::list<MixPresentationObu*> supported_mix_presentations =
      GetSupportedMixPresentations(
          descriptors_.ia_sequence_header.GetPrimaryProfile(),
          desired_profile_versions, descriptors_.audio_elements,
          descriptors_.mix_presentation_obus);
  if (supported_mix_presentations.empty()) {
    return absl::NotFoundError("No supported mix presentation OBUs found.");
  }
  absl::StatusOr<SelectedMixPresentation> selected_mix_presentation =
      FindMixPresentationAndLayout(descriptors_.audio_elements,
                                   supported_mix_presentations, desired_layout,
                                   desired_mix_presentation_id);
  if (!selected_mix_presentation.ok()) {
    return selected_mix_presentation.status();
  }
  // Cache the information.
  decoding_layout_info_ = {
      .mix_presentation_id =
          selected_mix_presentation->mix_presentation->GetMixPresentationId(),
      .layout = selected_mix_presentation->output_layout};

  // Even though the bitstream may have many mixes and layouts, `ObuProcessor`
  // exposes an interface to render one of them at a time.
  //
  // Clone a simplified version of the selected mix presentation, so clients do
  // not pay for mixes they cannot observe.
  absl::StatusOr<MixPresentationObu> simplified_mix_presentation =
      CreateSimplifiedMixPresentationForRendering(
          *selected_mix_presentation->mix_presentation,
          selected_mix_presentation->sub_mix_index,
          selected_mix_presentation->layout_index);
  if (!simplified_mix_presentation.ok()) {
    return simplified_mix_presentation.status();
  }

  // Configure simplified audio pipeline, from the simplified mix presentation.
  absl::StatusOr<RenderingModels> rendering_models =
      ConfigureSimplifiedAudioProcessingPipeline(descriptors_.audio_elements,
                                                 *simplified_mix_presentation,
                                                 trimming_settings);
  if (!rendering_models.ok()) {
    return rendering_models.status();
  }
  rendering_models_.emplace(*std::move(rendering_models));
  return absl::OkStatus();
}

absl::Status ObuProcessor::ProcessTemporalUnit(
    bool eos_is_end_of_sequence,
    std::optional<OutputTemporalUnit>& output_temporal_unit,
    bool& continue_processing) {
  RETURN_IF_NOT_OK(
      ValidateHasValue(temporal_unit_processor_, "temporal_unit_processor_"));
  return temporal_unit_processor_->ProcessTemporalUnit(
      eos_is_end_of_sequence, output_temporal_unit, continue_processing);
}

absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
ObuProcessor::RenderTemporalUnitAndMeasureLoudness(
    InternalTimestamp start_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    std::list<AudioFrameWithData>& audio_frames) {
  if (audio_frames.empty()) {
    // Nothing to decode, render, or measure loudness of. Consider this
    // trivially OK.
    return absl::Span<const absl::Span<const InternalSampleType>>();
  }
  if (!rendering_models_.has_value()) {
    return absl::FailedPreconditionError(
        "Not initialized for rendering. Did you call "
        "`CreateForRendering()`?");
  }

  // Decode the temporal unit.
  std::optional<InternalTimestamp> end_timestamp;
  for (auto& audio_frame : audio_frames) {
    // `ObuProcessor` renders only a single mix. Substreams may be irrelevant,
    // and the end-user should not pay to decode them.
    if (!rendering_models_->relevant_substream_ids.contains(
            audio_frame.obu.GetSubstreamId())) {
      continue;
    }
    if (!end_timestamp.has_value()) {
      end_timestamp = audio_frame.end_timestamp;
    }
    RETURN_IF_NOT_OK(
        CompareTimestamps(start_timestamp, audio_frame.start_timestamp,
                          "Audio frame has a different start timestamp than "
                          "the temporal unit: "));
    RETURN_IF_NOT_OK(CompareTimestamps(*end_timestamp,
                                       audio_frame.end_timestamp,
                                       "Audio frame has a different end "
                                       "timestamp than the temporal unit: "));
    RETURN_IF_NOT_OK(
        rendering_models_->audio_frame_decoder.Decode(audio_frame));
  }
  if (!end_timestamp.has_value()) {
    return absl::InvalidArgumentError(
        "No relevant audio frames in the temporal unit.");
  }

  // Reconstruct the temporal unit and store the result in the output map.
  const auto& decoded_labeled_frames_for_temporal_unit =
      rendering_models_->demixing_manager.DemixDecodedAudioSamples(
          audio_frames);
  if (!decoded_labeled_frames_for_temporal_unit.ok()) {
    return decoded_labeled_frames_for_temporal_unit.status();
  }

  RETURN_IF_NOT_OK(
      rendering_models_->mix_presentation_finalizer.PushTemporalUnit(
          *decoded_labeled_frames_for_temporal_unit, start_timestamp,
          *end_timestamp, parameter_blocks));

  // `ObuProcessor` renders a simplified Mix Presentation OBU with a single
  // sub-mix and a single layout.
  constexpr int kSubMixIndex = 0;
  constexpr int kLayoutIndex = 0;
  auto rendered_samples = rendering_models_->mix_presentation_finalizer
                              .GetPostProcessedSamplesAsSpan(
                                  decoding_layout_info_.mix_presentation_id,
                                  kSubMixIndex, kLayoutIndex);
  if (!rendered_samples.ok()) {
    return rendered_samples.status();
  }

  RETURN_IF_NOT_OK(ValidateRenderedSamples(
      *rendered_samples,
      output_frame_size_.has_value() ? *output_frame_size_ : 0));

  // TODO(b/379122580): Add a call to `FinalizePushingTemporalUnits`, then a
  //                    final call to `GetPostProcessedSamplesAsSpan` when there
  //                    are no more temporal units to push. Those calls may
  //                    belong elsewhere in the class depending on the
  //                    interface.
  return *rendered_samples;
}

absl::StatusOr<ObuProcessor::RenderingModels>
ObuProcessor::ConfigureSimplifiedAudioProcessingPipeline(
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& simplified_mix_presentation,
    TrimmingSettings trimming_settings) {
  // The audio elements IDs that are relevant to the selected mix presentation.
  absl::flat_hash_set<DecodedUleb128> relevant_audio_element_ids;
  for (const auto& sub_mix : simplified_mix_presentation.sub_mixes_) {
    for (const auto& audio_element : sub_mix.audio_elements) {
      relevant_audio_element_ids.insert(audio_element.audio_element_id);
    }
  }

  // Configure the `AudioFrameDecoder`, and prepare the structure which
  // configures the `DemixingManager`. Filter out any irrelevant audio
  // elements. Also cache any irrelevant substream IDs to be filtered out in
  // temporal units.
  AudioFrameDecoder audio_frame_decoder;
  absl::flat_hash_map<DecodedUleb128, DemixingManager::ReconstructionConfig>
      id_to_reconstruction_config;
  absl::flat_hash_set<DecodedUleb128> relevant_substream_ids;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    if (!relevant_audio_element_ids.contains(audio_element_id)) {
      continue;
    }
    relevant_substream_ids.insert(
        audio_element_with_data.obu.audio_substream_ids_.begin(),
        audio_element_with_data.obu.audio_substream_ids_.end());
    RETURN_IF_NOT_OK(audio_frame_decoder.InitDecodersForSubstreams(
        audio_element_with_data.substream_id_to_labels,
        *audio_element_with_data.codec_config));
    id_to_reconstruction_config[audio_element_id] = {
        .audio_element_obu = &audio_element_with_data.obu,
        .substream_id_to_labels =
            audio_element_with_data.substream_id_to_labels,
        .label_to_output_gain = audio_element_with_data.label_to_output_gain};
  }

  absl::StatusOr<DemixingManager> demixing_manager =
      DemixingManager::Create(id_to_reconstruction_config);
  if (!demixing_manager.ok()) {
    return demixing_manager.status();
  }

  // Create the mix presentation finalizer which is used to render the output
  // files. We neither trust the user-provided loudness, nor care about the
  // calculated loudness.
  absl::StatusOr<RenderingMixPresentationFinalizer> mix_presentation_finalizer =
      RenderingMixPresentationFinalizer::Create(
          trimming_settings, /*loudness_calculator_factory=*/nullptr,
          audio_elements,
          RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
          {simplified_mix_presentation});
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }

  return RenderingModels{
      .relevant_substream_ids = std::move(relevant_substream_ids),
      .audio_frame_decoder = std::move(audio_frame_decoder),
      .demixing_manager = *std::move(demixing_manager),
      .mix_presentation_finalizer = *std::move(mix_presentation_finalizer),
  };
}

}  // namespace iamf_tools
