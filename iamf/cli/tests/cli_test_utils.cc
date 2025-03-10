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
#include "iamf/cli/tests/cli_test_utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <list>
#include <memory>
#include <numbers>
#include <numeric>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/io/zero_copy_stream_impl.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {

namespace {

constexpr bool kOverrideAudioRollDistance = true;

void SetParamDefinitionCommonFields(DecodedUleb128 parameter_id,
                                    DecodedUleb128 parameter_rate,
                                    DecodedUleb128 duration,
                                    ParamDefinition& param_definition) {
  param_definition.parameter_id_ = parameter_id;
  param_definition.parameter_rate_ = parameter_rate;
  param_definition.param_definition_mode_ = 0;
  param_definition.reserved_ = 0;
  param_definition.duration_ = duration;
  param_definition.constant_subblock_duration_ = duration;
}

template <typename ParamDefinitionType>
void AddParamDefinition(DecodedUleb128 parameter_id,
                        DecodedUleb128 parameter_rate, DecodedUleb128 duration,
                        AudioElementObu& audio_element_obu,
                        ParamDefinitionType& param_definition) {
  SetParamDefinitionCommonFields(parameter_id, parameter_rate, duration,
                                 param_definition);

  // Add to the Audio Element OBU.
  audio_element_obu.InitializeParams(audio_element_obu.num_parameters_ + 1);
  audio_element_obu.audio_element_params_.emplace_back(
      AudioElementParam{param_definition});
}

}  // namespace

using ::absl_testing::IsOk;

absl::Status CollectObusFromIaSequence(
    ReadBitBuffer& read_bit_buffer, IASequenceHeaderObu& ia_sequence_header,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentations,
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks) {
  bool insufficient_data = false;
  auto obu_processor = ObuProcessor::Create(
      /*is_exhaustive_and_exact=*/false, &read_bit_buffer, insufficient_data);
  EXPECT_FALSE(insufficient_data);

  bool continue_processing = true;
  int temporal_unit_count = 0;
  LOG(INFO) << "Starting Temporal Unit OBU processing";
  while (continue_processing) {
    std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
    RETURN_IF_NOT_OK(obu_processor->ProcessTemporalUnit(
        /*eos_is_end_of_sequence=*/true, output_temporal_unit,
        continue_processing));
    audio_frames.splice(audio_frames.end(),
                        output_temporal_unit->output_audio_frames);
    parameter_blocks.splice(parameter_blocks.end(),
                            output_temporal_unit->output_parameter_blocks);
    temporal_unit_count++;
  }
  LOG(INFO) << "Processed " << temporal_unit_count << " Temporal Unit OBUs";

  // Move the processed data to the output.
  ia_sequence_header = obu_processor->ia_sequence_header_;
  codec_config_obus.swap(obu_processor->codec_config_obus_);
  audio_elements.swap(obu_processor->audio_elements_);
  mix_presentations.swap(obu_processor->mix_presentations_);
  return absl::OkStatus();
}

void AddLpcmCodecConfigWithIdAndSampleRate(
    uint32_t codec_config_id, uint32_t sample_rate,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdLpcm,
       .num_samples_per_frame = 8,
       .decoder_config = LpcmDecoderConfig{
           .sample_format_flags_bitmask_ = LpcmDecoderConfig::kLpcmLittleEndian,
           .sample_size_ = 16,
           .sample_rate_ = sample_rate}});
  EXPECT_THAT(obu.Initialize(kOverrideAudioRollDistance), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddOpusCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdOpus,
       .num_samples_per_frame = 8,
       .decoder_config = OpusDecoderConfig{
           .version_ = 1, .pre_skip_ = 312, .input_sample_rate_ = 0}});
  ASSERT_THAT(obu.Initialize(kOverrideAudioRollDistance), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddFlacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdFlac,
       .num_samples_per_frame = 16,
       .decoder_config = FlacDecoderConfig(
           {{{.header = {.last_metadata_block_flag = true,
                         .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                         .metadata_data_block_length = 34},
              .payload =
                  FlacMetaBlockStreamInfo{.minimum_block_size = 16,
                                          .maximum_block_size = 16,
                                          .sample_rate = 48000,
                                          .bits_per_sample = 15,
                                          .total_samples_in_stream = 0}}}})});
  ASSERT_THAT(obu.Initialize(kOverrideAudioRollDistance), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddAacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(ObuHeader(), codec_config_id,
                     {.codec_id = CodecConfig::kCodecIdAacLc,
                      .num_samples_per_frame = 1024,
                      .decoder_config = AacDecoderConfig{}});
  ASSERT_THAT(obu.Initialize(kOverrideAudioRollDistance), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddAmbisonicsMonoAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    absl::Span<const DecodedUleb128> substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  // Check the `codec_config_id` is known and this is a new
  // `audio_element_id`.
  auto codec_config_iter = codec_config_obus.find(codec_config_id);
  ASSERT_NE(codec_config_iter, codec_config_obus.end());
  ASSERT_EQ(audio_elements.find(audio_element_id), audio_elements.end());

  // Initialize the Audio Element OBU without any parameters.
  AudioElementObu obu = AudioElementObu(
      ObuHeader(), audio_element_id, AudioElementObu::kAudioElementSceneBased,
      0, codec_config_id);
  obu.InitializeParams(0);
  obu.InitializeAudioSubstreams(substream_ids.size());
  obu.audio_substream_ids_.assign(substream_ids.begin(), substream_ids.end());

  // Initialize to n-th order ambisonics. Choose the lowest order that can fit
  // all `substream_ids`. This may result in mixed-order ambisonics.
  uint8_t next_valid_output_channel_count;
  ASSERT_THAT(AmbisonicsConfig::GetNextValidOutputChannelCount(
                  substream_ids.size(), next_valid_output_channel_count),
              IsOk());
  EXPECT_THAT(obu.InitializeAmbisonicsMono(next_valid_output_channel_count,
                                           substream_ids.size()),
              IsOk());

  auto& channel_mapping =
      std::get<AmbisonicsMonoConfig>(
          std::get<AmbisonicsConfig>(obu.config_).ambisonics_config)
          .channel_mapping;
  // Map the first n channels from [0, n] in input order. Leave the rest of
  // the channels as unmapped.
  std::fill(channel_mapping.begin(), channel_mapping.end(),
            AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber);
  std::iota(channel_mapping.begin(),
            channel_mapping.begin() + substream_ids.size(), 0);

  AudioElementWithData audio_element = {
      .obu = std::move(obu), .codec_config = &codec_config_iter->second};
  ASSERT_THAT(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
                  audio_element.obu, audio_element.substream_id_to_labels),
              IsOk());

  audio_elements.emplace(audio_element_id, std::move(audio_element));
}

// Adds a scalable Audio Element OBU based on the input arguments.
void AddScalableAudioElementWithSubstreamIds(
    IamfInputLayout input_layout, DecodedUleb128 audio_element_id,
    uint32_t codec_config_id, absl::Span<const DecodedUleb128> substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::AudioElementObuMetadata>
      audio_element_metadatas;
  AudioElementMetadataBuilder builder;

  auto& new_audio_element_metadata = *audio_element_metadatas.Add();
  ASSERT_THAT(builder.PopulateAudioElementMetadata(
                  audio_element_id, codec_config_id, input_layout,
                  new_audio_element_metadata),
              IsOk());
  // Check that this is a scalable Audio Element, and override the substream
  // IDs.
  ASSERT_TRUE(new_audio_element_metadata.has_scalable_channel_layout_config());
  ASSERT_EQ(new_audio_element_metadata.num_substreams(), substream_ids.size());
  for (int i = 0; i < substream_ids.size(); ++i) {
    new_audio_element_metadata.mutable_audio_substream_ids()->Set(
        i, substream_ids[i]);
  }

  // Generate the Audio Element OBU.
  AudioElementGenerator generator(audio_element_metadatas);
  ASSERT_THAT(generator.Generate(codec_config_obus, audio_elements), IsOk());
}

void AddMixPresentationObuWithAudioElementIds(
    DecodedUleb128 mix_presentation_id,
    const std::vector<DecodedUleb128>& audio_element_ids,
    DecodedUleb128 common_parameter_id, DecodedUleb128 common_parameter_rate,
    std::list<MixPresentationObu>& mix_presentations) {
  MixGainParamDefinition common_mix_gain_param_definition;
  common_mix_gain_param_definition.parameter_id_ = common_parameter_id;
  common_mix_gain_param_definition.parameter_rate_ = common_parameter_rate;
  common_mix_gain_param_definition.param_definition_mode_ = true;
  common_mix_gain_param_definition.default_mix_gain_ = 0;

  // Configure one of the simplest mix presentation. Mix presentations REQUIRE
  // at least one sub-mix and a stereo layout.
  std::vector<MixPresentationSubMix> sub_mixes = {
      {.num_audio_elements =
           static_cast<DecodedUleb128>(audio_element_ids.size()),
       .output_mix_gain = common_mix_gain_param_definition,
       .num_layouts = 1,
       .layouts = {
           {.loudness_layout =
                {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
                 .specific_layout =
                     LoudspeakersSsConventionLayout{
                         .sound_system = LoudspeakersSsConventionLayout::
                             kSoundSystemA_0_2_0,
                         .reserved = 0}},
            .loudness = {.info_type = 0,
                         .integrated_loudness = 0,
                         .digital_peak = 0}}}}};
  for (const auto& audio_element_id : audio_element_ids) {
    sub_mixes[0].audio_elements.push_back({
        .audio_element_id = audio_element_id,
        .localized_element_annotations = {},
        .rendering_config =
            {.headphones_rendering_mode =
                 RenderingConfig::kHeadphonesRenderingModeStereo,
             .reserved = 0,
             .rendering_config_extension_size = 0,
             .rendering_config_extension_bytes = {}},
        .element_mix_gain = common_mix_gain_param_definition,
    });
  }

  mix_presentations.push_back(MixPresentationObu(
      ObuHeader(), mix_presentation_id,
      /*count_label=*/0, {}, {}, sub_mixes.size(), sub_mixes));
}

void AddParamDefinitionWithMode0AndOneSubblock(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration,
    absl::flat_hash_map<DecodedUleb128, MixGainParamDefinition>&
        param_definitions) {
  MixGainParamDefinition param_definition;
  SetParamDefinitionCommonFields(parameter_id, parameter_rate, duration,
                                 param_definition);
  param_definitions.emplace(parameter_id, param_definition);
}

void AddDemixingParamDefinition(DecodedUleb128 parameter_id,
                                DecodedUleb128 parameter_rate,
                                DecodedUleb128 duration,
                                AudioElementObu& audio_element_obu) {
  DemixingParamDefinition param_definition;

  // Specific fields of demixing param definitions.
  param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  param_definition.default_demixing_info_parameter_data_.reserved = 0;
  param_definition.default_demixing_info_parameter_data_.default_w = 10;
  param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;

  AddParamDefinition(parameter_id, parameter_rate, duration, audio_element_obu,
                     param_definition);
}

void AddReconGainParamDefinition(DecodedUleb128 parameter_id,
                                 DecodedUleb128 parameter_rate,
                                 DecodedUleb128 duration,
                                 AudioElementObu& audio_element_obu) {
  ReconGainParamDefinition param_definition(
      audio_element_obu.GetAudioElementId());

  AddParamDefinition(parameter_id, parameter_rate, duration, audio_element_obu,
                     param_definition);
}

WavReader CreateWavReaderExpectOk(const std::string& filename,
                                  int num_samples_per_frame) {
  auto wav_reader = WavReader::CreateFromFile(filename, num_samples_per_frame);
  EXPECT_THAT(wav_reader, IsOk());
  return std::move(*wav_reader);
}

void RenderAndFlushExpectOk(const LabeledFrame& labeled_frame,
                            AudioElementRendererBase* renderer,
                            std::vector<InternalSampleType>& output_samples) {
  ASSERT_NE(renderer, nullptr);
  EXPECT_THAT(renderer->RenderLabeledFrame(labeled_frame), IsOk());
  EXPECT_THAT(renderer->Finalize(), IsOk());
  EXPECT_TRUE(renderer->IsFinalized());
  EXPECT_THAT(renderer->Flush(output_samples), IsOk());
}

std::string GetAndCleanupOutputFileName(absl::string_view suffix) {
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  std::string file_name =
      absl::StrCat(test_info->name(), "-", test_info->test_suite_name(), "-",
                   test_info->test_case_name(), suffix);

  // It is possible that the test suite name and test case name contain the '/'
  // character. Replace it with '-' to form a legal file name.
  std::transform(file_name.begin(), file_name.end(), file_name.begin(),
                 [](char c) { return (c == '/') ? '-' : c; });
  const std::filesystem::path test_specific_file_name =
      std::filesystem::path(::testing::TempDir()) / file_name;

  std::filesystem::remove(test_specific_file_name);
  return test_specific_file_name.string();
}

std::string GetAndCreateOutputDirectory(absl::string_view suffix) {
  const std::string output_directory = GetAndCleanupOutputFileName(suffix);
  std::error_code error_code;
  EXPECT_TRUE(
      std::filesystem::create_directories(output_directory, error_code));
  return output_directory;
}

std::vector<uint8_t> SerializeObusExpectOk(
    const std::list<const ObuBase*>& obus, const LebGenerator& leb_generator) {
  using ::absl_testing::IsOk;
  WriteBitBuffer serialized_obus(0, leb_generator);
  for (const auto* obu : obus) {
    EXPECT_NE(obu, nullptr);
    EXPECT_THAT(obu->ValidateAndWriteObu(serialized_obus), IsOk());
  }

  return serialized_obus.bit_buffer();
}

void ParseUserMetadataAssertSuccess(
    const std::string& textproto_filename,
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(std::filesystem::exists(textproto_filename));
  std::ifstream user_metadata_file(textproto_filename, std::ios::in);
  google::protobuf::io::IstreamInputStream input_stream(&user_metadata_file);
  ASSERT_TRUE(
      google::protobuf::TextFormat::Parse(&input_stream, &user_metadata));
}

double GetLogSpectralDistance(
    const absl::Span<const InternalSampleType>& first_log_spectrum,
    const absl::Span<const InternalSampleType>& second_log_spectrum) {
  const int num_samples = first_log_spectrum.size();
  if (num_samples != second_log_spectrum.size()) {
    LOG(ERROR) << "Spectrum sizes are not equal.";
    return false;
  }
  double log_spectral_distance = 0.0;
  for (int i = 0; i < num_samples; ++i) {
    log_spectral_distance += (first_log_spectrum[i] - second_log_spectrum[i]) *
                             (first_log_spectrum[i] - second_log_spectrum[i]);
  }
  return (10 * std::sqrt(log_spectral_distance / num_samples));
}

std::vector<DecodeSpecification> GetDecodeSpecifications(
    const iamf_tools_cli_proto::UserMetadata& user_metadata) {
  std::vector<DecodeSpecification> decode_specifications;
  for (const auto& mix_presentation :
       user_metadata.mix_presentation_metadata()) {
    for (int i = 0; i < mix_presentation.num_sub_mixes(); ++i) {
      for (int j = 0; j < mix_presentation.sub_mixes(i).num_layouts(); ++j) {
        DecodeSpecification decode_specification;
        decode_specification.mix_presentation_id =
            mix_presentation.mix_presentation_id();
        decode_specification.sub_mix_index = i;
        if (mix_presentation.sub_mixes(i)
                .layouts(j)
                .loudness_layout()
                .has_ss_layout()) {
          auto sound_system_status = MixPresentationGenerator::CopySoundSystem(
              mix_presentation.sub_mixes(i)
                  .layouts(j)
                  .loudness_layout()
                  .ss_layout()
                  .sound_system(),
              decode_specification.sound_system);
          if (!sound_system_status.ok()) {
            LOG(ERROR) << "Failed to copy sound system: "
                       << sound_system_status;
            continue;
          }
        }
        decode_specification.layout_index = j;
        decode_specifications.push_back(decode_specification);
      }
    }
  }
  return decode_specifications;
}

std::vector<InternalSampleType> Int32ToInternalSampleType(
    absl::Span<const int32_t> samples) {
  std::vector<InternalSampleType> result(samples.size());
  Int32ToInternalSampleType(samples, absl::MakeSpan(result));
  return result;
}

std::vector<InternalSampleType> GenerateSineWav(uint64_t start_tick,
                                                uint32_t num_samples,
                                                uint32_t sample_rate_hz,
                                                double frequency_hz,
                                                double amplitude) {
  std::vector<InternalSampleType> samples(num_samples, 0.0);
  constexpr double kPi = std::numbers::pi_v<InternalSampleType>;
  const double time_base = 1.0 / sample_rate_hz;

  for (int frame_tick = 0; frame_tick < num_samples; ++frame_tick) {
    const double t = start_tick + frame_tick;
    samples[frame_tick] =
        amplitude * sin(2.0 * kPi * frequency_hz * t * time_base);
  }
  return samples;
}

void AccumulateZeroCrossings(
    absl::Span<const std::vector<int32_t>> samples,
    std::vector<ZeroCrossingState>& zero_crossing_states,
    std::vector<int>& zero_crossing_counts) {
  using enum ZeroCrossingState;
  const auto num_channels = samples.empty() ? 0 : samples[0].size();
  // Seed the data structures, or check they contain the right number of
  // channels.
  if (zero_crossing_counts.empty()) {
    zero_crossing_counts.resize(num_channels, 0);
  } else {
    ASSERT_EQ(num_channels, zero_crossing_counts.size());
  }
  if (zero_crossing_states.empty()) {
    zero_crossing_states.resize(num_channels, ZeroCrossingState::kUnknown);
  } else {
    ASSERT_EQ(num_channels, zero_crossing_states.size());
  }

  // Zero crossing threshold determined empirically for -18 dB sine waves to
  // skip encoding artifacts (e.g. a small ringing artifact < -40 dB after
  // the sine wave stopped.)  Note that -18 dB would correspond to dividing
  // by 8, while dividing by 100 is -40 dB.
  constexpr int32_t kThreshold = std::numeric_limits<int32_t>::max() / 100;
  for (const auto& tick : samples) {
    ASSERT_EQ(tick.size(), num_channels);
    for (int i = 0; i < num_channels; ++i) {
      ZeroCrossingState next_state = (tick[i] > kThreshold)    ? kPositive
                                     : (tick[i] < -kThreshold) ? kNegative
                                                               : kUnknown;
      if (next_state == kUnknown) {
        // Don't do anything if it's not clearly positive or negative.
        continue;
      } else if (zero_crossing_states[i] != next_state) {
        // If we clearly flipped states, count it as a zero crossing.
        zero_crossing_counts[i]++;
        zero_crossing_states[i] = next_state;
      }
    }
  }
}

absl::Status ReadFileToBytes(const std::filesystem::path& file_path,
                             std::vector<uint8_t>& buffer) {
  if (!std::filesystem::exists(file_path)) {
    return absl::NotFoundError("File not found.");
  }
  std::ifstream ifs(file_path, std::ios::binary | std::ios::in);

  // Increase the size of the buffer. Write to the original end (before
  // resizing).
  const auto file_size = std::filesystem::file_size(file_path);
  const auto original_buffer_size = buffer.size();
  buffer.resize(original_buffer_size + file_size);
  ifs.read(reinterpret_cast<char*>(buffer.data() + original_buffer_size),
           file_size);
  return absl::OkStatus();
}

absl::Status EverySecondTickResampler::PushFrameDerived(
    absl::Span<const std::vector<int32_t>> time_channel_samples) {
  EXPECT_EQ(num_valid_ticks_, 0);  // `SampleProcessorBase` should ensure this.
  for (size_t i = 0; i < time_channel_samples.size(); ++i) {
    if (i % 2 == 1) {
      output_time_channel_samples_[num_valid_ticks_] = time_channel_samples[i];
      ++num_valid_ticks_;
    }
  }
  return absl::OkStatus();
}

absl::Status EverySecondTickResampler::FlushDerived() {
  EXPECT_EQ(num_valid_ticks_, 0);  // `SampleProcessorBase` should ensure this.
  return absl::OkStatus();
}

absl::Status OneFrameDelayer::PushFrameDerived(
    absl::Span<const std::vector<int32_t>> time_channel_samples) {
  // Swap the delayed samples with the output samples from the base class.
  std::swap(delayed_samples_, output_time_channel_samples_);
  std::swap(num_delayed_ticks_, num_valid_ticks_);

  // The fact that the input size is less than the output size should have
  // already been validated in `SampleProcessorBase`, but for safety we can
  // check it here.
  EXPECT_LE(time_channel_samples.size(), delayed_samples_.size());
  // Cache the new samples to delay.
  std::copy(time_channel_samples.begin(), time_channel_samples.end(),
            delayed_samples_.begin());
  num_delayed_ticks_ = time_channel_samples.size();

  return absl::OkStatus();
}

absl::Status OneFrameDelayer::FlushDerived() {
  // Pushing in an empty frame will cause the delayed frame to be available.
  return PushFrameDerived({});
}

}  // namespace iamf_tools
