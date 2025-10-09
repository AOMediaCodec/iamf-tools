/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/audio_frame_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/codec_config_obu_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr DecodedUleb128 kCodecConfigId = 99;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kSubstreamId = 0;
constexpr uint8_t kSampleSize = 16;
constexpr uint32_t kSampleRate = 48000;

void ConfigureUserMetadata(iamf_tools_cli_proto::UserMetadata& user_metadata,
                           int num_samples_per_frame) {
  auto& codec_config_metadata = *user_metadata.add_codec_config_metadata();
  codec_config_metadata =
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, num_samples_per_frame, kSampleSize, kSampleRate);
  codec_config_metadata.mutable_codec_config()->set_audio_roll_distance(0);

  auto& audio_frame_metadata = *user_metadata.add_audio_frame_metadata();
  ABSL_CHECK_OK(AudioFrameMetadataBuilder::PopulateAudioFrameMetadata(
      /*wav_filename=*/"", kAudioElementId, IamfInputLayout::kStereo,
      audio_frame_metadata));

  AudioElementMetadataBuilder audio_element_builder;
  auto& audio_element_metadata = *user_metadata.add_audio_element_metadata();
  ABSL_CHECK_OK(audio_element_builder.PopulateAudioElementMetadata(
      kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
      audio_element_metadata));
  // Override with the custom substream ID.
  audio_element_metadata.set_audio_substream_ids(0, kSubstreamId);
}

void InitializeAudioFrameGenerator(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const absl::flat_hash_map<uint32_t, ParamDefinitionVariant>&
        param_definitions,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::unique_ptr<GlobalTimingModule>& global_timing_module,
    std::optional<ParametersManager>& parameters_manager,
    std::optional<AudioFrameGenerator>& audio_frame_generator) {
  // Initialize pre-requisite OBUs and the global timing module. This is all
  // derived from the `user_metadata`.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  ABSL_CHECK_OK(codec_config_generator.Generate(codec_config_obus));

  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  ABSL_CHECK_OK(
      audio_element_generator.Generate(codec_config_obus, audio_elements));

  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ABSL_CHECK_OK(demixing_module);
  global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definitions);
  ABSL_CHECK_NE(global_timing_module, nullptr);

  parameters_manager.emplace(audio_elements);
  ABSL_CHECK(parameters_manager.has_value());
  ABSL_CHECK_OK(parameters_manager->Initialize());

  // Create an audio frame generator.
  audio_frame_generator.emplace(user_metadata.audio_frame_metadata(),
                                user_metadata.codec_config_metadata(),
                                audio_elements, *demixing_module,
                                *parameters_manager, *global_timing_module);
  ABSL_CHECK(audio_frame_generator.has_value());

  // Initialize.
  ABSL_CHECK_OK(audio_frame_generator->Initialize());
}

static void BM_AddSamples(benchmark::State& state) {
  // Set up the input.
  const int num_samples_per_frame = state.range(0);
  const std::vector<InternalSampleType> l2_samples(num_samples_per_frame, 0.5);
  const std::vector<InternalSampleType> r2_samples(num_samples_per_frame, -0.5);
  const absl::flat_hash_map<ChannelLabel::Label,
                            absl::Span<const InternalSampleType>>
      label_to_frame = {{ChannelLabel::kL2, l2_samples},
                        {ChannelLabel::kR2, r2_samples}};

  // Prepare an audio frame generator.
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureUserMetadata(user_metadata, num_samples_per_frame);
  const absl::flat_hash_map<uint32_t, ParamDefinitionVariant>
      param_definitions = {};
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  std::unique_ptr<GlobalTimingModule> global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(
      user_metadata, param_definitions, codec_config_obus, audio_elements,
      global_timing_module, parameters_manager, audio_frame_generator);

  // Measure the calls to `AudioFrameGenerator::AddSamples()`.
  for (auto _ : state) {
    for (const auto& [label, frame] : label_to_frame) {
      ABSL_CHECK_OK(
          audio_frame_generator->AddSamples(kAudioElementId, label, frame));
    }
  }
}

// Benchmark with different number of samples per frame.
BENCHMARK(BM_AddSamples)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12})
    ->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace iamf_tools
