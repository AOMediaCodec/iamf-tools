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
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
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
#include "iamf/cli/proto/encoder_control_metadata.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"
#include "iamf/cli/proto_conversion/proto_to_obu/arbitrary_obu_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_generator.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definition_variant.h"
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

// Arranges the `ArbitraryObu`s into the non-timestamped (descriptor) and
// timestamped lists.
void ArrangeArbitraryObus(
    std::list<ArbitraryObu>& original_arbitrary_obus,
    std::list<ArbitraryObu>& descriptor_arbitrary_obus,
    absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&
        timestamp_to_arbitrary_obus) {
  while (!original_arbitrary_obus.empty()) {
    auto& arbitrary_obu = original_arbitrary_obus.front();
    // Arrange to the timestamps slot (if present), or the untimestamped slot,
    // which implies the OBUs are descriptor OBUs.
    auto& list_to_move_to =
        arbitrary_obu.insertion_tick_.has_value()
            ? timestamp_to_arbitrary_obus[*arbitrary_obu.insertion_tick_]
            : descriptor_arbitrary_obus;
    list_to_move_to.push_back(std::move(arbitrary_obu));
    original_arbitrary_obus.pop_front();
  }
}

void SpliceArbitraryObus(
    auto iter_to_splice,
    absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&
        timestamp_to_arbitrary_obus,
    std::list<ArbitraryObu>& temporal_unit_arbitrary_obus) {
  if (iter_to_splice == timestamp_to_arbitrary_obus.end()) {
    return;
  }
  temporal_unit_arbitrary_obus.splice(temporal_unit_arbitrary_obus.end(),
                                      iter_to_splice->second);
  timestamp_to_arbitrary_obus.erase(iter_to_splice);
}

}  // namespace

absl::StatusOr<IamfEncoder> IamfEncoder::Create(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const RendererFactoryBase* /* absl_nullable */ renderer_factory,
    const LoudnessCalculatorFactoryBase* /* absl_nullable */
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata.ia_sequence_header_metadata_size() != 1) {
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
  }

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));
  RETURN_IF_NOT_OK(
      ValidateHasValue(ia_sequence_header_obu, "IA Sequence Header"));

  // Codec Config OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto codec_config_obus =
      std::make_unique<absl::flat_hash_map<DecodedUleb128, CodecConfigObu>>();
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(*codec_config_obus));

  // Audio Element OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto audio_elements = std::make_unique<
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>();
  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(*codec_config_obus, *audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated after all temporal units have been pushed.
  std::list<MixPresentationObu> mix_presentation_obus;
  MixPresentationGenerator mix_presentation_generator(
      user_metadata.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(
      user_metadata.encoder_control_metadata().add_build_information_tag(),
      mix_presentation_obus));
  // Initialize a mix presentation mix presentation finalizer. Requires
  // rendering data for every submix to accurately compute loudness.
  auto mix_presentation_finalizer = RenderingMixPresentationFinalizer::Create(
      renderer_factory, loudness_calculator_factory, *audio_elements,
      sample_processor_factory, mix_presentation_obus);
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }

  // Generate Arbitrary OBUs.
  std::list<ArbitraryObu> unorganized_arbitrary_obus;
  ArbitraryObuGenerator arbitrary_obu_generator(
      user_metadata.arbitrary_obu_metadata());
  RETURN_IF_NOT_OK(
      arbitrary_obu_generator.Generate(unorganized_arbitrary_obus));
  // Arrange the `ArbitraryObu`s into the non-timestamped (descriptor) and
  // timestamped lists.
  std::list<ArbitraryObu> descriptor_arbitrary_obus;
  absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>
      timestamp_to_arbitrary_obus;
  ArrangeArbitraryObus(unorganized_arbitrary_obus, descriptor_arbitrary_obus,
                       timestamp_to_arbitrary_obus);

  // Collect and validate consistency of all `ParamDefinition`s in all
  // Audio Element and Mix Presentation OBUs.
  auto param_definition_variants = std::make_unique<
      absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>>();

  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      *audio_elements, mix_presentation_obus, *param_definition_variants));

  // Initialize the global timing module.
  auto global_timing_module =
      GlobalTimingModule::Create(*audio_elements, *param_definition_variants);
  if (global_timing_module == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }

  // Initialize the parameter block generator.
  ParameterBlockGenerator parameter_block_generator(
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      *param_definition_variants);
  RETURN_IF_NOT_OK(parameter_block_generator.Initialize(*audio_elements));

  // Put generated parameter blocks in a manager that supports easier queries.
  auto parameters_manager =
      std::make_unique<ParametersManager>(*audio_elements);
  RETURN_IF_NOT_OK(parameters_manager->Initialize());

  // Down-mix the audio samples and then demix audio samples while decoding
  // them. This is useful to create multi-layer audio elements and to determine
  // the recon gain parameters and to measuring loudness.
  const absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      audio_element_id_to_demixing_metadata =
          CreateAudioElementIdToDemixingMetadata(user_metadata,
                                                 *audio_elements);
  if (!audio_element_id_to_demixing_metadata.ok()) {
    return audio_element_id_to_demixing_metadata.status();
  }
  auto demixing_module = DemixingModule::CreateForDownMixingAndReconstruction(
      *std::move(audio_element_id_to_demixing_metadata));
  if (!demixing_module.ok()) {
    return demixing_module.status();
  }

  auto audio_frame_generator = std::make_unique<AudioFrameGenerator>(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), *audio_elements, *demixing_module,
      *parameters_manager, *global_timing_module);
  RETURN_IF_NOT_OK(audio_frame_generator->Initialize());

  // Initialize the audio frame decoder. It is needed to determine the recon
  // gain parameters and measure the loudness of the mixes.
  AudioFrameDecoder audio_frame_decoder;
  RETURN_IF_NOT_OK(InitAudioFrameDecoderForAllAudioElements(
      *audio_elements, audio_frame_decoder));

  // Construct the `IamfEncoder`. Move various OBUs, models, etc. into it.
  return IamfEncoder(
      user_metadata.test_vector_metadata().validate_user_loudness(),
      *std::move(ia_sequence_header_obu), std::move(codec_config_obus),
      std::move(audio_elements), std::move(mix_presentation_obus),
      std::move(descriptor_arbitrary_obus),
      std::move(timestamp_to_arbitrary_obus),
      std::move(param_definition_variants),
      std::move(parameter_block_generator), std::move(parameters_manager),
      *demixing_module, std::move(audio_frame_generator),
      std::move(audio_frame_decoder), std::move(global_timing_module),
      std::move(*mix_presentation_finalizer));
}

bool IamfEncoder::GeneratingDataObus() const {
  // Once the `AudioFrameGenerator` is done, and there are no more extraneous
  // timestamped arbitrary OBUs, we are done.
  return (audio_frame_generator_ != nullptr) &&
         (audio_frame_generator_->TakingSamples() ||
          audio_frame_generator_->GeneratingFrames() ||
          !timestamp_to_arbitrary_obus_.empty());
}

void IamfEncoder::BeginTemporalUnit() {
  // Clear cached samples for this iteration of data OBU generation.
  for (auto& [audio_element_id, labeled_samples] : id_to_labeled_samples_) {
    for (auto& [label, samples] : labeled_samples) {
      samples.clear();
    }
  }
}

absl::Status IamfEncoder::GetInputTimestamp(
    InternalTimestamp& input_timestamp) {
  std::optional<InternalTimestamp> timestamp;
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
                             absl::Span<const InternalSampleType> samples) {
  if (add_samples_finalized_) {
    LOG_FIRST_N(WARNING, 3)
        << "Calling `AddSamples()` after `FinalizeAddSamples()` has no effect; "
        << samples.size() << " input samples discarded.";
    return;
  }

  id_to_labeled_samples_[audio_element_id][label] =
      std::vector<InternalSampleType>(samples.begin(), samples.end());
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
    std::list<ParameterBlockWithData>& parameter_blocks,
    std::list<ArbitraryObu>& temporal_unit_arbitrary_obus) {
  audio_frames.clear();
  parameter_blocks.clear();
  temporal_unit_arbitrary_obus.clear();

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
    if (add_samples_finalized_) {
      // At the end of the sequence, there could be some extraneous arbitrary
      // OBUs that are not associated with any audio frames. Pop the next set.
      SpliceArbitraryObus(timestamp_to_arbitrary_obus_.begin(),
                          timestamp_to_arbitrary_obus_,
                          temporal_unit_arbitrary_obus);
    } else {
      // Some audio codec will only output an encoded frame after the next
      // frame "pushes" the old one out. So we wait until the next iteration to
      // retrieve it.
      LOG(INFO) << "No audio frames generated";
    }
    return absl::OkStatus();
  }
  // All generated audio frame should be in the same temporal unit; they all
  // have the same timestamps.
  const InternalTimestamp output_start_timestamp =
      audio_frames.front().start_timestamp;
  const InternalTimestamp output_end_timestamp =
      audio_frames.front().end_timestamp;

  // Decode the audio frames in place. The decoded samples are required to
  // determine the demixed frames.
  for (auto& audio_frame : audio_frames) {
    RETURN_IF_NOT_OK(audio_frame_decoder_.Decode(audio_frame));
    CHECK_EQ(output_start_timestamp, audio_frame.start_timestamp);
    CHECK_EQ(output_end_timestamp, audio_frame.end_timestamp);
  }

  // Demix the original and decoded audio frames, differences between them are
  // useful to compute the recon gain parameters.
  const auto id_to_labeled_frame =
      demixing_module_.DemixOriginalAudioSamples(audio_frames);
  if (!id_to_labeled_frame.ok()) {
    return id_to_labeled_frame.status();
  }
  const auto id_to_labeled_decoded_frame =
      demixing_module_.DemixDecodedAudioSamples(audio_frames);
  if (!id_to_labeled_decoded_frame.ok()) {
    return id_to_labeled_decoded_frame.status();
  }

  // Recon gain parameter blocks are generated based on the original and
  // demixed audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateReconGain(
      *id_to_labeled_frame, *id_to_labeled_decoded_frame,
      *global_timing_module_, temp_recon_gain_parameter_blocks_));

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

  // Pop out the arbitrary OBUs belonging to this temporal unit.
  SpliceArbitraryObus(timestamp_to_arbitrary_obus_.find(output_start_timestamp),
                      timestamp_to_arbitrary_obus_,
                      temporal_unit_arbitrary_obus);

  return mix_presentation_finalizer_.PushTemporalUnit(
      *id_to_labeled_frame, output_start_timestamp, output_end_timestamp,
      parameter_blocks);
}

const IASequenceHeaderObu& IamfEncoder::GetIaSequenceHeaderObu() const {
  return ia_sequence_header_obu_;
}

const absl::flat_hash_map<uint32_t, CodecConfigObu>&
IamfEncoder::GetCodecConfigObus() const {
  return *codec_config_obus_;
}

const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
IamfEncoder::GetAudioElements() const {
  return *audio_elements_;
}

const std::list<MixPresentationObu>&
IamfEncoder::GetPreliminaryMixPresentationObus() const {
  return mix_presentation_obus_;
}

const std::list<ArbitraryObu>& IamfEncoder::GetDescriptorArbitraryObus() const {
  return descriptor_arbitrary_obus_;
}

absl::StatusOr<std::list<MixPresentationObu>>
IamfEncoder::GetFinalizedMixPresentationObus() {
  if (GeneratingDataObus()) {
    return absl::FailedPreconditionError(
        "Cannot finalize mix presentation OBUs while generating data OBUs.");
  }

  RETURN_IF_NOT_OK(mix_presentation_finalizer_.FinalizePushingTemporalUnits());
  return mix_presentation_finalizer_.GetFinalizedMixPresentationObus(
      validate_user_loudness_);
}

}  // namespace iamf_tools
