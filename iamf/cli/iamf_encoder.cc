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
#include "iamf/cli/iamf_encoder.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/arbitrary_obu_generator.h"
#include "iamf/cli/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_to_obu/parameter_block_generator.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status InitAudioFrameDecoderForAllAudioElements(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    AudioFrameDecoder& audio_frame_decoder) {
  for (const auto& [unused_audio_element_id, audio_element] : audio_elements) {
    if (audio_element.codec_config == nullptr) {
      // Skip stray audio elements. We won't know how to decode their
      // substreams.
      continue;
    }

    RETURN_IF_NOT_OK(audio_frame_decoder.InitDecodersForSubstreams(
        audio_element.substream_id_to_labels, *audio_element.codec_config));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<IamfEncoder> IamfEncoder::Create(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    absl::Nullable<const RendererFactoryBase*> renderer_factory,
    absl::Nullable<const LoudnessCalculatorFactoryBase*>
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    std::optional<IASequenceHeaderObu>& ia_sequence_header_obu,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus,
    std::list<ArbitraryObu>& arbitrary_obus) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata.ia_sequence_header_metadata_size() != 1) {
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
  }
  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));

  // Codec Config OBUs.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(codec_config_obus));

  // Audio Element OBUs.
  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(codec_config_obus, audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated later.
  MixPresentationGenerator mix_presentation_generator(
      user_metadata.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(mix_presentation_obus));
  // Initialize a mix presentation mix presentation finalizer. Requires
  // rendering data for every submix to accurately compute loudness.
  auto mix_presentation_finalizer = RenderingMixPresentationFinalizer::Create(
      renderer_factory, loudness_calculator_factory, audio_elements,
      sample_processor_factory, mix_presentation_obus);
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }

  // Generate Arbitrary OBUs.
  ArbitraryObuGenerator arbitrary_obu_generator(
      user_metadata.arbitrary_obu_metadata());
  RETURN_IF_NOT_OK(arbitrary_obu_generator.Generate(arbitrary_obus));

  // Collect and validate consistency of all `ParamDefinition`s in all
  // Audio Element and Mix Presentation OBUs.
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements, mix_presentation_obus, param_definitions));

  // Initialize the global timing module.
  auto global_timing_module = std::make_unique<GlobalTimingModule>();
  RETURN_IF_NOT_OK(
      global_timing_module->Initialize(audio_elements, param_definitions));

  // Initialize the parameter block generator.
  auto parameter_id_to_metadata = std::make_unique<
      absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>>();
  ParameterBlockGenerator parameter_block_generator(
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      *parameter_id_to_metadata);
  RETURN_IF_NOT_OK(
      parameter_block_generator.Initialize(audio_elements, param_definitions));

  // Put generated parameter blocks in a manager that supports easier queries.
  auto parameters_manager = std::make_unique<ParametersManager>(audio_elements);
  RETURN_IF_NOT_OK(parameters_manager->Initialize());

  // Down-mix the audio samples and then demix audio samples while decoding
  // them. This is useful to create multi-layer audio elements and to determine
  // the recon gain parameters and to measuring loudness.
  auto demixing_module = DemixingModule::CreateForDownMixingAndReconstruction(
      user_metadata, audio_elements);
  if (!demixing_module.ok()) {
    return demixing_module.status();
  }

  auto audio_frame_generator = std::make_unique<AudioFrameGenerator>(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), audio_elements, *demixing_module,
      *parameters_manager, *global_timing_module);
  RETURN_IF_NOT_OK(audio_frame_generator->Initialize());

  // Initialize the audio frame decoder. It is needed to determine the recon
  // gain parameters and measure the loudness of the mixes.
  AudioFrameDecoder audio_frame_decoder;
  RETURN_IF_NOT_OK(InitAudioFrameDecoderForAllAudioElements(
      audio_elements, audio_frame_decoder));

  return IamfEncoder(
      user_metadata.test_vector_metadata().validate_user_loudness(),
      std::move(parameter_id_to_metadata), std::move(param_definitions),
      std::move(parameter_block_generator), std::move(parameters_manager),
      *demixing_module, std::move(audio_frame_generator),
      std::move(audio_frame_decoder), std::move(global_timing_module),
      std::move(*mix_presentation_finalizer));
}

bool IamfEncoder::GeneratingDataObus() const {
  return (audio_frame_generator_ != nullptr) &&
         (audio_frame_generator_->TakingSamples() ||
          audio_frame_generator_->GeneratingFrames());
}

void IamfEncoder::BeginTemporalUnit() {
  // Clear cached samples for this iteration of data OBU generation.
  for (auto& [audio_element_id, labeled_samples] : id_to_labeled_samples_) {
    for (auto& [label, samples] : labeled_samples) {
      samples.clear();
    }
  }
}

absl::Status IamfEncoder::GetInputTimestamp(int32_t& input_timestamp) {
  std::optional<int32_t> timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module_->GetGlobalAudioFrameTimestamp(timestamp));
  if (!timestamp.has_value()) {
    return absl::InvalidArgumentError("Global timestamp has no value");
  }
  input_timestamp = *timestamp;
  return absl::OkStatus();
}

void IamfEncoder::AddSamples(const DecodedUleb128 audio_element_id,
                             ChannelLabel::Label label,
                             const std::vector<InternalSampleType>& samples) {
  if (add_samples_finalized_) {
    LOG_FIRST_N(WARNING, 3)
        << "Calling `AddSamples()` after `FinalizeAddSamples()` has no effect; "
        << samples.size() << " input samples discarded.";
    return;
  }

  id_to_labeled_samples_[audio_element_id][label] = samples;
}

void IamfEncoder::FinalizeAddSamples() { add_samples_finalized_ = true; }

absl::Status IamfEncoder::AddParameterBlockMetadata(
    const iamf_tools_cli_proto::ParameterBlockObuMetadata&
        parameter_block_metadata) {
  RETURN_IF_NOT_OK(
      parameter_block_generator_.AddMetadata(parameter_block_metadata));
  return absl::OkStatus();
}

absl::Status IamfEncoder::OutputTemporalUnit(
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks) {
  audio_frames.clear();
  parameter_blocks.clear();

  // Generate mix gain and demixing parameter blocks.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateDemixing(
      *global_timing_module_, temp_demixing_parameter_blocks_));
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateMixGain(
      *global_timing_module_, temp_mix_gain_parameter_blocks_));

  // Add the newly generated demixing parameter blocks to the parameters
  // manager so they can be easily queried by the audio frame generator.
  for (const auto& demixing_parameter_block : temp_demixing_parameter_blocks_) {
    parameters_manager_->AddDemixingParameterBlock(&demixing_parameter_block);
  }

  for (const auto& [audio_element_id, labeled_samples] :
       id_to_labeled_samples_) {
    for (const auto& [label, samples] : labeled_samples) {
      // Skip adding empty `samples` to the audio frame generator.
      if (samples.empty()) {
        continue;
      }
      RETURN_IF_NOT_OK(
          audio_frame_generator_->AddSamples(audio_element_id, label, samples));
    }
  }

  if (add_samples_finalized_) {
    RETURN_IF_NOT_OK(audio_frame_generator_->Finalize());
  }

  RETURN_IF_NOT_OK(audio_frame_generator_->OutputFrames(audio_frames));
  if (audio_frames.empty()) {
    // Some audio codec will only output an encoded frame after the next
    // frame "pushes" the old one out. So we wait till the next iteration to
    // retrieve it.
    return absl::OkStatus();
  }
  // All generated audio frame should be in the same temporal unit; they all
  // have the same timestamps.
  const int32_t output_start_timestamp = audio_frames.front().start_timestamp;
  const int32_t output_end_timestamp = audio_frames.front().end_timestamp;

  // Decode the audio frames. They are required to determine the demixed
  // frames.
  std::list<DecodedAudioFrame> decoded_audio_frames;
  for (const auto& audio_frame : audio_frames) {
    auto decoded_audio_frame = audio_frame_decoder_.Decode(audio_frame);
    if (!decoded_audio_frame.ok()) {
      return decoded_audio_frame.status();
    }
    CHECK_EQ(output_start_timestamp, decoded_audio_frame->start_timestamp);
    CHECK_EQ(output_end_timestamp, decoded_audio_frame->end_timestamp);
    decoded_audio_frames.emplace_back(*decoded_audio_frame);
  }

  // Demix the audio frames.
  IdLabeledFrameMap id_to_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  RETURN_IF_NOT_OK(demixing_module_.DemixAudioSamples(
      audio_frames, decoded_audio_frames, id_to_labeled_frame,
      id_to_labeled_decoded_frame));

  // Recon gain parameter blocks are generated based on the original and
  // demixed audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateReconGain(
      id_to_labeled_frame, id_to_labeled_decoded_frame, *global_timing_module_,
      temp_recon_gain_parameter_blocks_));

  // Move all generated parameter blocks belonging to this temporal unit to
  // the output.
  for (auto* temp_parameter_blocks :
       {&temp_mix_gain_parameter_blocks_, &temp_demixing_parameter_blocks_,
        &temp_recon_gain_parameter_blocks_}) {
    auto last_same_timestamp_iter = std::find_if(
        temp_parameter_blocks->begin(), temp_parameter_blocks->end(),
        [output_start_timestamp](const auto& parameter_block) {
          return parameter_block.start_timestamp > output_start_timestamp;
        });
    parameter_blocks.splice(parameter_blocks.end(), *temp_parameter_blocks,
                            temp_parameter_blocks->begin(),
                            last_same_timestamp_iter);
  }

  return mix_presentation_finalizer_.PushTemporalUnit(
      id_to_labeled_frame, output_start_timestamp, output_end_timestamp,
      parameter_blocks);
}

absl::Status IamfEncoder::FinalizeMixPresentationObus(
    std::list<MixPresentationObu>& mix_presentation_obus) {
  if (GeneratingDataObus()) {
    return absl::FailedPreconditionError(
        "Cannot finalize mix presentation OBUs while generating data OBUs.");
  }

  return mix_presentation_finalizer_.Finalize(validate_user_loudness_,
                                              mix_presentation_obus);
}

}  // namespace iamf_tools
