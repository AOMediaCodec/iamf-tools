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

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;

constexpr DecodedUleb128 kFirstAudioElementId = DecodedUleb128(1);
constexpr DecodedUleb128 kSecondAudioElementId = DecodedUleb128(2);
constexpr DecodedUleb128 kFirstCodecConfigId = DecodedUleb128(11);
constexpr DecodedUleb128 kSecondCodecConfigId = DecodedUleb128(12);
constexpr DecodedUleb128 kFirstSubstreamId = DecodedUleb128(21);
constexpr DecodedUleb128 kSecondSubstreamId = DecodedUleb128(22);
constexpr DecodedUleb128 kFirstParameterId = DecodedUleb128(31);
constexpr DecodedUleb128 kSecondParameterId = DecodedUleb128(32);
constexpr std::array<uint8_t, 12> kFirstReconGainValues = {
    255, 0, 125, 200, 150, 255, 255, 255, 255, 255, 255, 255};
constexpr std::array<uint8_t, 12> kSecondReconGainValues = {
    0, 1, 2, 3, 4, 255, 255, 255, 255, 255, 255, 255};

constexpr int32_t kStartTimestamp = 0;
constexpr int32_t kEndTimestamp = 8;
constexpr int32_t kDuration = 8;

TEST(GenerateAudioElementWithData, ValidAudioElementWithCodecConfig) {
  absl::flat_hash_map<DecodedUleb128, AudioElementObu> audio_element_obus;
  audio_element_obus.emplace(
      kFirstAudioElementId,
      AudioElementObu(
          ObuHeader(), kFirstAudioElementId,
          AudioElementObu::AudioElementType::kAudioElementChannelBased,
          /*reserved=*/0, kFirstCodecConfigId));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  codec_config_obus.emplace(
      kFirstCodecConfigId,
      CodecConfigObu(ObuHeader(), kFirstCodecConfigId, CodecConfig()));
  absl::StatusOr<absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>
      audio_element_with_data_map =
          ObuWithDataGenerator::GenerateAudioElementsWithData(
              codec_config_obus, audio_element_obus);
  EXPECT_THAT(audio_element_with_data_map, IsOk());
  EXPECT_EQ((*audio_element_with_data_map).size(), 1);

  auto iter = codec_config_obus.find(kFirstCodecConfigId);
  const CodecConfigObu* expected_codec_config_obu = &iter->second;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      expected_audio_element_with_data_map;
  expected_audio_element_with_data_map.emplace(
      kFirstAudioElementId,
      AudioElementWithData{
          .obu = AudioElementObu(
              ObuHeader(), kFirstAudioElementId,
              AudioElementObu::AudioElementType::kAudioElementChannelBased,
              /*reserved=*/0, kFirstCodecConfigId),
          .codec_config = expected_codec_config_obu,
          .substream_id_to_labels = {},
          .label_to_output_gain = {},
          .channel_numbers_for_layers = {}});
  EXPECT_EQ(expected_audio_element_with_data_map,
            audio_element_with_data_map.value());
}

TEST(GenerateAudioElementWithData, MultipleAudioElementsWithOneCodecConfig) {
  absl::flat_hash_map<DecodedUleb128, AudioElementObu> audio_element_obus;
  audio_element_obus.emplace(
      kFirstAudioElementId,
      AudioElementObu(
          ObuHeader(), kFirstAudioElementId,
          AudioElementObu::AudioElementType::kAudioElementChannelBased,
          /*reserved=*/0, kFirstCodecConfigId));
  audio_element_obus.emplace(
      kSecondAudioElementId,
      AudioElementObu(
          ObuHeader(), kSecondAudioElementId,
          AudioElementObu::AudioElementType::kAudioElementChannelBased,
          /*reserved=*/0, kFirstCodecConfigId));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  codec_config_obus.emplace(
      kFirstCodecConfigId,
      CodecConfigObu(ObuHeader(), kFirstCodecConfigId, CodecConfig()));
  absl::StatusOr<absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>
      audio_element_with_data_map =
          ObuWithDataGenerator::GenerateAudioElementsWithData(
              codec_config_obus, audio_element_obus);
  EXPECT_THAT(audio_element_with_data_map, IsOk());
  EXPECT_EQ((*audio_element_with_data_map).size(), 2);

  auto iter = codec_config_obus.find(kFirstCodecConfigId);
  const CodecConfigObu* expected_codec_config_obu = &iter->second;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      expected_audio_element_with_data_map;
  expected_audio_element_with_data_map.emplace(
      kFirstAudioElementId,
      AudioElementWithData{
          .obu = AudioElementObu(
              ObuHeader(), kFirstAudioElementId,
              AudioElementObu::AudioElementType::kAudioElementChannelBased,
              /*reserved=*/0, kFirstCodecConfigId),
          .codec_config = expected_codec_config_obu,
          .substream_id_to_labels{},
          .label_to_output_gain{},
          .channel_numbers_for_layers{}});
  expected_audio_element_with_data_map.emplace(
      kSecondAudioElementId,
      AudioElementWithData{
          .obu = AudioElementObu(
              ObuHeader(), kSecondAudioElementId,
              AudioElementObu::AudioElementType::kAudioElementChannelBased,
              /*reserved=*/0, kFirstCodecConfigId),
          .codec_config = expected_codec_config_obu,
          .substream_id_to_labels = {},
          .label_to_output_gain = {},
          .channel_numbers_for_layers = {}});
  EXPECT_EQ(expected_audio_element_with_data_map,
            audio_element_with_data_map.value());
}

TEST(GenerateAudioElementWithData, InvalidCodecConfigId) {
  absl::flat_hash_map<DecodedUleb128, AudioElementObu> audio_element_obus;
  audio_element_obus.emplace(
      kFirstAudioElementId,
      AudioElementObu(
          ObuHeader(), kFirstAudioElementId,
          AudioElementObu::AudioElementType::kAudioElementChannelBased,
          /*reserved=*/0, kSecondCodecConfigId));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  codec_config_obus.emplace(
      kFirstCodecConfigId,
      CodecConfigObu(ObuHeader(), kFirstCodecConfigId, CodecConfig()));
  absl::StatusOr<absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>
      audio_element_with_data_map =
          ObuWithDataGenerator::GenerateAudioElementsWithData(
              codec_config_obus, audio_element_obus);
  EXPECT_FALSE(audio_element_with_data_map.ok());
}

// TODO(b/377772983): `ObuWithDataGenerator::GenerateAudioFrameWithData()` works
//                    on individual frames and may not have the knowledge of
//                    the "global state" of the whole bitstream. So any test
//                    that tests the global state should be moved to the user
//                    of the function, namely `ObuProcessor`.
class GenerateAudioFrameWithDataTest : public testing::Test {
 public:
  // Used to compare down mixing params.
  struct AlphaBetaGammaDelta {
    const double alpha;
    const double beta;
    const double gamma;
    const double delta;
  };

  GenerateAudioFrameWithDataTest()
      : kObuHeader{.obu_type = kObuIaAudioFrame,
                   .num_samples_to_trim_at_end = 1,
                   .num_samples_to_trim_at_start = 1},
        kAudioFrameData{1, 2, 3},
        kFirstSubstreamAudioFrameObu(kObuHeader, kFirstSubstreamId,
                                     kAudioFrameData),
        kSecondSubstreamAudioFrameObu(kObuHeader, kSecondSubstreamId,
                                      kAudioFrameData) {}

 protected:
  void SetUpObus(
      const std::vector<DecodedUleb128>& substream_ids,
      const std::vector<AudioFrameObu>& audio_frame_obus_per_substream,
      int num_frames_per_substream) {
    AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kFirstAudioElementId, kFirstCodecConfigId, substream_ids,
        codec_config_obus_, audio_elements_with_data_);

    ASSERT_EQ(substream_ids.size(), audio_frame_obus_per_substream.size());
    for (int j = 0; j < num_frames_per_substream; j++) {
      for (int i = 0; i < substream_ids.size(); i++) {
        audio_frame_obus_.push_back(audio_frame_obus_per_substream[i]);
      }
    }
  }

  void AddDemixingAudioParam(DemixingInfoParameterData::DMixPMode dmixp_mode,
                             DecodedUleb128 parameter_id) {
    DemixingParamDefinition param_definition;
    FillCommonParamDefinition(parameter_id, param_definition);

    param_definition.default_demixing_info_parameter_data_.dmixp_mode =
        dmixp_mode;
    param_definition.default_demixing_info_parameter_data_.default_w = 0;
    AudioElementParam param = {
        .param_definition_type = ParamDefinition::kParameterDefinitionDemixing,
        .param_definition =
            std::make_unique<DemixingParamDefinition>(param_definition)};
    AddAudioParam(parameter_id,
                  DemixingParamDefinition::kParameterDefinitionDemixing,
                  std::move(param), param_definition);
  }

  void AddReconGainAudioParam(DecodedUleb128 parameter_id) {
    ReconGainParamDefinition param_definition =
        ReconGainParamDefinition(kFirstAudioElementId);
    FillCommonParamDefinition(parameter_id, param_definition);

    AudioElementParam param = {
        .param_definition_type = ParamDefinition::kParameterDefinitionReconGain,
        .param_definition =
            std::make_unique<ReconGainParamDefinition>(param_definition)};
    AddAudioParam(parameter_id,
                  DemixingParamDefinition::kParameterDefinitionReconGain,
                  std::move(param), param_definition);
  }

  void SetUpModules() {
    // Set up the global timing module.
    ASSERT_THAT(global_timing_module_.Initialize(audio_elements_with_data_,
                                                 param_definitions_),
                IsOk());

    // Set up the parameters manager.
    parameters_manager_ =
        std::make_unique<ParametersManager>(audio_elements_with_data_);
    ASSERT_THAT(parameters_manager_->Initialize(), IsOk());
  }

  void SetUpParameterBlockWithData(
      const std::optional<DecodedUleb128>& recon_gain_parameter_id,
      const std::vector<std::array<uint8_t, 12>>& recon_gain_values_vector,
      const std::optional<DecodedUleb128>& demixing_parameter_id,
      const std::vector<DemixingInfoParameterData::DMixPMode>&
          dmixp_mode_vector) {
    std::list<std::unique_ptr<ParameterBlockObu>> parameter_block_obus;
    const int num_ids = (recon_gain_parameter_id.has_value() ? 1 : 0) +
                        (demixing_parameter_id.has_value() ? 1 : 0);

    std::optional<PerIdParameterMetadata> recon_gain_per_id_metadata =
        std::nullopt;
    if (recon_gain_parameter_id.has_value()) {
      recon_gain_per_id_metadata =
          parameter_id_to_metadata_[*recon_gain_parameter_id];
    }
    std::optional<PerIdParameterMetadata> demixing_per_id_metadata =
        std::nullopt;
    if (demixing_parameter_id.has_value()) {
      demixing_per_id_metadata =
          parameter_id_to_metadata_[*demixing_parameter_id];
    }

    // Add parameter block OBUs in temporal order.
    for (int i = 0;
         i < recon_gain_values_vector.size() || i < dmixp_mode_vector.size();
         i++) {
      if (recon_gain_parameter_id.has_value()) {
        parameter_block_obus.push_back(std::make_unique<ParameterBlockObu>(
            ObuHeader(), *recon_gain_parameter_id,
            *recon_gain_per_id_metadata));
        EXPECT_THAT(parameter_block_obus.back()->InitializeSubblocks(), IsOk());

        // Data specific to recon gain parameter blocks.
        auto recon_gain_info_parameter_data =
            std::make_unique<ReconGainInfoParameterData>();
        recon_gain_info_parameter_data->recon_gain_elements.push_back(
            ReconGainElement{.recon_gain_flag = DecodedUleb128(1),
                             .recon_gain = recon_gain_values_vector[i]});
        parameter_block_obus.back()->subblocks_[0].param_data =
            std::move(recon_gain_info_parameter_data);
      }
      if (demixing_parameter_id.has_value()) {
        parameter_block_obus.push_back(std::make_unique<ParameterBlockObu>(
            ObuHeader(), *demixing_parameter_id, *demixing_per_id_metadata));
        EXPECT_THAT(parameter_block_obus.back()->InitializeSubblocks(), IsOk());

        // Data specific to demixing parameter blocks.
        auto demixing_parameter_data =
            std::make_unique<DemixingInfoParameterData>();
        demixing_parameter_data->dmixp_mode = dmixp_mode_vector[i];
        demixing_parameter_data->reserved = 0;
        parameter_block_obus.back()->subblocks_[0].param_data =
            std::move(demixing_parameter_data);
      }
    }

    // Call `GenerateParameterBlockWithData()` iteratively with one OBU at a
    // time.
    absl::flat_hash_map<DecodedUleb128, int32_t>
        parameter_id_to_last_end_timestamp;
    absl::flat_hash_map<DecodedUleb128, int> parameter_blocks_count;
    for (auto& parameter_block_obu : parameter_block_obus) {
      const auto& parameter_id = parameter_block_obu->parameter_id_;
      auto [last_end_timestamp_iter, unused_inserted] =
          parameter_id_to_last_end_timestamp.insert(
              {parameter_id, kStartTimestamp});
      auto parameter_block_with_data =
          ObuWithDataGenerator::GenerateParameterBlockWithData(
              last_end_timestamp_iter->second, global_timing_module_,
              std::move(parameter_block_obu));
      ASSERT_THAT(parameter_block_with_data, IsOk());
      last_end_timestamp_iter->second =
          parameter_block_with_data->end_timestamp;
      parameter_blocks_with_data_.push_back(
          std::move(*parameter_block_with_data));
      parameter_blocks_count[parameter_id]++;
    }

    ASSERT_EQ(parameter_blocks_count.size(), num_ids);
    if (recon_gain_parameter_id.has_value()) {
      auto iter = parameter_blocks_count.find(*recon_gain_parameter_id);
      ASSERT_NE(iter, parameter_blocks_count.end());
      ASSERT_EQ(iter->second, recon_gain_values_vector.size());
    }
    if (demixing_parameter_id.has_value()) {
      auto iter = parameter_blocks_count.find(*demixing_parameter_id);
      ASSERT_NE(iter, parameter_blocks_count.end());
      ASSERT_EQ(iter->second, dmixp_mode_vector.size());
    }
  }

  // Add parameter blocks with data belonging to the same temporal unit to
  // the parameters manager.
  void AddCurrentParameterBlocksToParametersManager(
      std::list<ParameterBlockWithData>::iterator& parameter_block_iter) {
    std::optional<int32_t> global_timestamp = std::nullopt;
    ASSERT_THAT(
        global_timing_module_.GetGlobalAudioFrameTimestamp(global_timestamp),
        IsOk());
    for (; parameter_block_iter != parameter_blocks_with_data_.end();
         parameter_block_iter++) {
      const auto& parameter_block = *parameter_block_iter;
      if (!global_timestamp.has_value() ||
          parameter_block.start_timestamp != *global_timestamp) {
        return;
      }
      auto param_definition_type =
          parameter_id_to_metadata_.at(parameter_block.obu->parameter_id_)
              .param_definition_type;
      if (param_definition_type ==
          ParamDefinition::kParameterDefinitionDemixing) {
        parameters_manager_->AddDemixingParameterBlock(&parameter_block);
      } else if (param_definition_type ==
                 ParamDefinition::kParameterDefinitionReconGain) {
        parameters_manager_->AddReconGainParameterBlock(&parameter_block);
      }
    }
  }

  void UpdateParameterStatesIfNeeded() {
    std::optional<int32_t> global_timestamp = std::nullopt;
    EXPECT_THAT(
        global_timing_module_.GetGlobalAudioFrameTimestamp(global_timestamp),
        IsOk());
    if (!global_timestamp.has_value()) {
      return;
    }
    EXPECT_THAT(parameters_manager_->UpdateDemixingState(kFirstAudioElementId,
                                                         *global_timestamp),
                IsOk());
    EXPECT_THAT(parameters_manager_->UpdateReconGainState(kFirstAudioElementId,
                                                          *global_timestamp),
                IsOk());
  }

  void ValidateAudioFrameWithData(
      const AudioFrameWithData& audio_frame_with_data,
      const AudioFrameObu& expected_audio_frame_obu,
      int32_t expected_start_timestamp, int32_t expected_end_timestamp,
      DecodedUleb128 audio_element_id) {
    EXPECT_EQ(audio_frame_with_data.obu, expected_audio_frame_obu);
    EXPECT_EQ(audio_frame_with_data.start_timestamp, expected_start_timestamp);
    EXPECT_EQ(audio_frame_with_data.end_timestamp, expected_end_timestamp);
    EXPECT_FALSE(audio_frame_with_data.pcm_samples.has_value());
    EXPECT_EQ(audio_frame_with_data.audio_element_with_data,
              &audio_elements_with_data_.at(audio_element_id));
  }

  void ValidateDownMimxingParams(const DownMixingParams& down_mixing_params,
                                 const AlphaBetaGammaDelta& expected_params) {
    EXPECT_TRUE(down_mixing_params.in_bitstream);
    EXPECT_FLOAT_EQ(down_mixing_params.alpha, expected_params.alpha);
    EXPECT_FLOAT_EQ(down_mixing_params.beta, expected_params.beta);
    EXPECT_FLOAT_EQ(down_mixing_params.gamma, expected_params.gamma);
    EXPECT_FLOAT_EQ(down_mixing_params.delta, expected_params.delta);
  }

  void ValidateReconGainParameters(
      const ReconGainInfoParameterData& recon_gain_info_parameter_data,
      const std::array<uint8_t, 12>& expected_recon_gain_values) {
    EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 1);
    const auto& recon_gain_element =
        recon_gain_info_parameter_data.recon_gain_elements[0];
    EXPECT_EQ(recon_gain_element.recon_gain_flag, DecodedUleb128(1));
    EXPECT_THAT(recon_gain_element.recon_gain,
                ElementsAreArray(expected_recon_gain_values));
  }

  const ObuHeader kObuHeader;
  const std::vector<uint8_t> kAudioFrameData;
  const AudioFrameObu kFirstSubstreamAudioFrameObu;
  const AudioFrameObu kSecondSubstreamAudioFrameObu;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data_;

  std::list<AudioFrameObu> audio_frame_obus_;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      param_definitions_;
  std::list<ParameterBlockWithData> parameter_blocks_with_data_;

  // Using `node_hash_map` because pointer stability is desired.
  absl::node_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata_;
  GlobalTimingModule global_timing_module_;
  std::unique_ptr<ParametersManager> parameters_manager_;

 private:
  void FillCommonParamDefinition(DecodedUleb128 parameter_id,
                                 ParamDefinition& param_definition) {
    param_definition.parameter_id_ = parameter_id;
    param_definition.param_definition_mode_ = 0;
    param_definition.duration_ = 8;
    param_definition.parameter_rate_ = 1;
    param_definition.InitializeSubblockDurations(1);
  }

  void AddAudioParam(
      DecodedUleb128 parameter_id,
      DemixingParamDefinition::ParameterDefinitionType param_definition_type,
      AudioElementParam&& param, const ParamDefinition& param_definition) {
    auto& audio_element_obu =
        audio_elements_with_data_.at(kFirstAudioElementId).obu;
    audio_element_obu.num_parameters_++;
    audio_element_obu.audio_element_params_.push_back(std::move(param));

    // Create per-ID metadata for this parameter.
    parameter_id_to_metadata_[parameter_id] =
        PerIdParameterMetadata{.param_definition_type = param_definition_type,
                               .param_definition = param_definition};
    param_definitions_.emplace(
        parameter_id,
        &parameter_id_to_metadata_.at(parameter_id).param_definition);
  }
};

TEST_F(GenerateAudioFrameWithDataTest, ValidAudioFrame) {
  // Set up inputs.
  SetUpObus({kFirstSubstreamId}, {kFirstSubstreamAudioFrameObu}, 1);
  SetUpModules();

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    ASSERT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
  }

  // Expectations.
  const auto& first_audio_frame_with_data = audio_frames_with_data.front();
  ValidateAudioFrameWithData(first_audio_frame_with_data,
                             kFirstSubstreamAudioFrameObu, kStartTimestamp,
                             kEndTimestamp, kFirstAudioElementId);

  // The audio element has no down mixing params. IAMF provides no guidance when
  // they are not present, but make sure they are sane in case they are used.
  // Check they generally near the range of pre-defined `dmixp_mode`s from
  // IAMF v1.1.0.
  EXPECT_FALSE(first_audio_frame_with_data.down_mixing_params.in_bitstream);
  EXPECT_GE(first_audio_frame_with_data.down_mixing_params.alpha, 0.5);
  EXPECT_LE(first_audio_frame_with_data.down_mixing_params.alpha, 1.0);
  EXPECT_GE(first_audio_frame_with_data.down_mixing_params.beta, 0.5);
  EXPECT_LE(first_audio_frame_with_data.down_mixing_params.beta, 1.0);
  EXPECT_GE(first_audio_frame_with_data.down_mixing_params.gamma, 0.5);
  EXPECT_LE(first_audio_frame_with_data.down_mixing_params.gamma, 1.0);
  EXPECT_GE(first_audio_frame_with_data.down_mixing_params.delta, 0.5);
  EXPECT_LE(first_audio_frame_with_data.down_mixing_params.delta, 1.0);
}

TEST_F(GenerateAudioFrameWithDataTest,
       ValidAudioFrameWithParamDefinitionDownMixingParams) {
  // Set up inputs.
  SetUpObus({kFirstSubstreamId}, {kFirstSubstreamAudioFrameObu}, 1);
  AddDemixingAudioParam(DemixingInfoParameterData::kDMixPMode2,
                        kFirstParameterId);
  SetUpModules();

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
  }

  // Expectations.
  const auto& first_audio_frame_with_data = audio_frames_with_data.front();
  ValidateAudioFrameWithData(first_audio_frame_with_data,
                             kFirstSubstreamAudioFrameObu, kStartTimestamp,
                             kEndTimestamp, kFirstAudioElementId);
  ValidateDownMimxingParams(first_audio_frame_with_data.down_mixing_params,
                            {0.707, 0.707, 0.707, 0.707});
}

TEST_F(GenerateAudioFrameWithDataTest,
       ValidAudioFramesWithMultipleParameterBlockDownMixingParams) {
  // 1 audio element with 1 substream and 2 audio frames, as there are 2
  // temporal units. The audio element had 1 param definition for demixing
  // params. There are 2 parameter blocks, one for each temporal unit. We
  // should generate 2 `AudioFramesWithData`, since there are 2 temporal units.

  // Set up inputs.
  SetUpObus({kFirstSubstreamId}, {kFirstSubstreamAudioFrameObu}, 2);
  AddDemixingAudioParam(DemixingInfoParameterData::kDMixPMode1,
                        kFirstParameterId);
  SetUpModules();
  SetUpParameterBlockWithData(
      /*recon_gain_parameter_id=*/std::nullopt,
      /*recon_gain_values_vector=*/{},
      /*demixing_parameter_id=*/kFirstParameterId,
      /*dmixp_mode_vector=*/
      {DemixingInfoParameterData::kDMixPMode2,
       DemixingInfoParameterData::kDMixPMode3});

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  auto parameter_block_iter = parameter_blocks_with_data_.begin();
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    AddCurrentParameterBlocksToParametersManager(parameter_block_iter);
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
    UpdateParameterStatesIfNeeded();
  }

  // Expectations.
  EXPECT_EQ(audio_frames_with_data.size(), 2);
  int32_t expected_start_timestamp = kStartTimestamp;
  int32_t expected_end_timestamp = kEndTimestamp;
  const std::vector<AlphaBetaGammaDelta> expected_alpha_beta_gamma_delta{
      AlphaBetaGammaDelta{0.707, 0.707, 0.707, 0.707},  // `kDMixPMode2`.
      AlphaBetaGammaDelta{1.0, 0.866, 0.866, 0.866}     // `kDMixPMode3`.
  };
  int frame_index = 0;
  for (const auto& audio_frame_with_data : audio_frames_with_data) {
    ValidateAudioFrameWithData(
        audio_frame_with_data, kFirstSubstreamAudioFrameObu,
        expected_start_timestamp, expected_end_timestamp, kFirstAudioElementId);
    ValidateDownMimxingParams(audio_frame_with_data.down_mixing_params,
                              expected_alpha_beta_gamma_delta[frame_index++]);
    expected_start_timestamp += 8;
    expected_end_timestamp += 8;
  }
}

TEST_F(GenerateAudioFrameWithDataTest,
       ValidAudioFramesInMultipleSubstreamsWithSameDownMixingParams) {
  // Multiple substreams should be in the same audio element.
  // That same audio element should have one param definition with the down
  // mixing param id. We should have 2 audio frames in each substream. This is
  // a total of 4 audio frames.
  // We will have 1 parameter block for each time stamp. This is a total of 2
  // parameter blocks. The same parameter block at a given timestamp should be
  // used for both substreams. This is a total of 2 temporal units.

  // Set up inputs.
  SetUpObus({kFirstSubstreamId, kSecondSubstreamId},
            {kFirstSubstreamAudioFrameObu, kSecondSubstreamAudioFrameObu}, 2);
  AddDemixingAudioParam(DemixingInfoParameterData::kDMixPMode1,
                        kFirstParameterId);
  SetUpModules();

  SetUpParameterBlockWithData(
      /*recon_gain_parameter_id=*/std::nullopt,
      /*recon_gain_values_vector=*/{},
      /*demixing_parameter_id=*/kFirstParameterId,
      /*dmixp_mode_vector=*/
      {DemixingInfoParameterData::kDMixPMode2,
       DemixingInfoParameterData::kDMixPMode3});

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  auto parameter_block_iter = parameter_blocks_with_data_.begin();
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    AddCurrentParameterBlocksToParametersManager(parameter_block_iter);
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
    UpdateParameterStatesIfNeeded();
  }

  // Expectations.
  // We should generate 4 `AudioFramesWithData`.
  EXPECT_EQ(audio_frames_with_data.size(), 4);

  // We will validate frames in the two substreams independently.
  // Frame indices corresponding to the two substreams.
  std::vector<int> frame_index_for_substreams = {0, 0};

  // Expected audio frame OBU corresponding to the two substreams.
  const std::vector<AudioFrameObu> expected_audio_frame_obu_for_substreams = {
      kFirstSubstreamAudioFrameObu, kSecondSubstreamAudioFrameObu};

  // Expected timestamps for successive temporal units. Same for both
  // substreams.
  const std::vector<int32_t> expected_start_timestamps = {kStartTimestamp,
                                                          kStartTimestamp + 8};
  const std::vector<int32_t> expected_end_timestamps = {kEndTimestamp,
                                                        kEndTimestamp + 8};

  // Expected {alpha, beta, gamma, delta} for successive temporal units. Same
  // for both substreams.
  const std::vector<AlphaBetaGammaDelta> expected_alpha_beta_gamma_delta{
      AlphaBetaGammaDelta{0.707, 0.707, 0.707, 0.707},  // `kDMixPMode2`.
      AlphaBetaGammaDelta{1.0, 0.866, 0.866, 0.866}     // `kDMixPMode3`.
  };
  for (auto& audio_frame_with_data : audio_frames_with_data) {
    int substream_index =
        (audio_frame_with_data.obu.GetSubstreamId() == kFirstSubstreamId) ? 0
                                                                          : 1;
    int& frame_index = frame_index_for_substreams[substream_index];
    ValidateAudioFrameWithData(
        audio_frame_with_data,
        expected_audio_frame_obu_for_substreams[substream_index],
        expected_start_timestamps[frame_index],
        expected_end_timestamps[frame_index], kFirstAudioElementId);
    ValidateDownMimxingParams(audio_frame_with_data.down_mixing_params,
                              expected_alpha_beta_gamma_delta[frame_index]);
    frame_index++;
  }
}

TEST_F(GenerateAudioFrameWithDataTest,
       ValidAudioFrameWithMultipleReconGainParams) {
  // 1 audio element with 1 substream and 2 audio frames, as there are 2
  // temporal units. The audio element had 1 param definition for recon gain
  // params. There are 2 parameter blocks, one for each temporal unit. We should
  // generate 2 `AudioFramesWithData`, since there are 2 temporal units.

  // Set up inputs.
  SetUpObus({kFirstSubstreamId}, {kFirstSubstreamAudioFrameObu}, 2);
  AddReconGainAudioParam(kFirstParameterId);
  SetUpModules();
  SetUpParameterBlockWithData(
      /*recon_gain_parameter_id=*/kFirstParameterId,
      /*recon_gain_values_vector=*/
      {kFirstReconGainValues, kSecondReconGainValues},
      /*demixing_parameter_id=*/std::nullopt,
      /*dmixp_mode_vector=*/{});

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  auto parameter_block_iter = parameter_blocks_with_data_.begin();
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    AddCurrentParameterBlocksToParametersManager(parameter_block_iter);
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
    UpdateParameterStatesIfNeeded();
  }

  // Expectations.
  EXPECT_EQ(audio_frames_with_data.size(), 2);
  int32_t expected_start_timestamp = kStartTimestamp;
  int32_t expected_end_timestamp = kEndTimestamp;
  const std::vector<std::array<uint8_t, 12>> expected_recon_gain_values = {
      kFirstReconGainValues, kSecondReconGainValues};
  int frame_index = 0;
  for (const auto& audio_frame_with_data : audio_frames_with_data) {
    ValidateAudioFrameWithData(
        audio_frame_with_data, kFirstSubstreamAudioFrameObu,
        expected_start_timestamp, expected_end_timestamp, kFirstAudioElementId);
    ValidateReconGainParameters(
        audio_frame_with_data.recon_gain_info_parameter_data,
        expected_recon_gain_values[frame_index++]);
    expected_start_timestamp += 8;
    expected_end_timestamp += 8;
  }
}

TEST_F(GenerateAudioFrameWithDataTest,
       ValidAudioFrameWithMultipleReconGainAndDemixingParams) {
  // 1 audio element with 1 substream and 2 audio frames, as there are 2
  // temporal units. The audio element had 1 param definition for recon gain
  // parameters and 1 param definition for demixing parameters. There are 4
  // parameter blocks, two for each temporal unit (one recon gain and one
  // demixing). We should generate 2 `AudioFramesWithData`, since there are 2
  // temporal units.

  // Set up inputs.
  SetUpObus({kFirstSubstreamId}, {kFirstSubstreamAudioFrameObu}, 2);
  AddReconGainAudioParam(kFirstParameterId);
  AddDemixingAudioParam(DemixingInfoParameterData::kDMixPMode1,
                        kSecondParameterId);
  SetUpModules();
  SetUpParameterBlockWithData(
      /*recon_gain_parameter_id=*/kFirstParameterId,
      /*recon_gain_values_vector=*/
      {kFirstReconGainValues, kSecondReconGainValues},
      /*demixing_parameter_id=*/kSecondParameterId,
      /*dmixp_mode_vector=*/
      {DemixingInfoParameterData::kDMixPMode2,
       DemixingInfoParameterData::kDMixPMode3});

  // Call `GenerateAudioFrameWithData()`.
  std::list<AudioFrameWithData> audio_frames_with_data;
  auto parameter_block_iter = parameter_blocks_with_data_.begin();
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    AddCurrentParameterBlocksToParametersManager(parameter_block_iter);
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_THAT(audio_frame_with_data, IsOk());
    audio_frames_with_data.push_back(std::move(*audio_frame_with_data));
    UpdateParameterStatesIfNeeded();
  }

  // Expectations.
  EXPECT_EQ(audio_frames_with_data.size(), 2);
  int32_t expected_start_timestamp = kStartTimestamp;
  int32_t expected_end_timestamp = kEndTimestamp;
  const std::vector<std::array<uint8_t, 12>> expected_recon_gain_values = {
      kFirstReconGainValues, kSecondReconGainValues};
  const std::vector<AlphaBetaGammaDelta> expected_alpha_beta_gamma_delta = {
      {0.707, 0.707, 0.707, 0.707},  // `kDMixPMode2`.
      {1.0, 0.866, 0.866, 0.866},    // `kDMixPMode3`.
  };
  int frame_index = 0;
  for (const auto& audio_frame_with_data : audio_frames_with_data) {
    ValidateAudioFrameWithData(
        audio_frame_with_data, kFirstSubstreamAudioFrameObu,
        expected_start_timestamp, expected_end_timestamp, kFirstAudioElementId);
    ValidateDownMimxingParams(audio_frame_with_data.down_mixing_params,
                              expected_alpha_beta_gamma_delta[frame_index]);
    ValidateReconGainParameters(
        audio_frame_with_data.recon_gain_info_parameter_data,
        expected_recon_gain_values[frame_index]);
    expected_start_timestamp += 8;
    expected_end_timestamp += 8;
    frame_index++;
  }
}

TEST_F(GenerateAudioFrameWithDataTest, RejectMismatchingAudioElement) {
  // Set up inputs. Notice that the substream ID recorded in the audio element
  // (`kSecondSubstreamId`) is different from that in the audio frame OBU
  // (`kFirstSubstreamId`). This will cause `GenerateAudioFrameWithData()`
  // to fail, because it cannot find the correspoinding audio element of the
  // audio frame being processed.
  SetUpObus({kSecondSubstreamId}, {kFirstSubstreamAudioFrameObu}, 1);
  SetUpModules();

  // Call `GenerateAudioFrameWithData()`.
  for (const auto& audio_frame_obu : audio_frame_obus_) {
    auto audio_frame_with_data =
        ObuWithDataGenerator::GenerateAudioFrameWithData(
            audio_elements_with_data_.at(kFirstAudioElementId), audio_frame_obu,
            global_timing_module_, *parameters_manager_);
    EXPECT_FALSE(audio_frame_with_data.ok());
  }
}

TEST(GenerateParameterBlockWithData, ValidParameterBlock) {
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      /*substream_ids=*/{kFirstSubstreamId}, codec_config_obus,
      audio_elements_with_data);

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  ParamDefinition param_definition = ParamDefinition();
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = static_cast<DecodedUleb128>(kDuration);
  param_definition.parameter_rate_ = 1;
  param_definitions.emplace(kFirstParameterId, &param_definition);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              param_definitions),
              IsOk());
  std::list<std::unique_ptr<ParameterBlockObu>> parameter_block_obus;
  PerIdParameterMetadata per_id_metadata = {.param_definition =
                                                param_definition};
  parameter_block_obus.push_back(std::make_unique<ParameterBlockObu>(
      ObuHeader(), kFirstParameterId, per_id_metadata));

  // Call `GenerateParameterBlockWithData()` iteratively with one OBU at a time.
  auto start_timestamp = kStartTimestamp;
  std::list<ParameterBlockWithData> parameter_blocks_with_data;
  for (auto& parameter_block_obu : parameter_block_obus) {
    auto parameter_block_with_data =
        ObuWithDataGenerator::GenerateParameterBlockWithData(
            start_timestamp, global_timing_module,
            std::move(parameter_block_obu));
    EXPECT_THAT(parameter_block_with_data, IsOk());
    start_timestamp += kDuration;
    parameter_blocks_with_data.push_back(std::move(*parameter_block_with_data));
  }

  // Set up expected output.
  EXPECT_EQ(parameter_blocks_with_data.size(), 1);
  EXPECT_EQ(parameter_blocks_with_data.front().obu->parameter_id_,
            kFirstParameterId);
  EXPECT_EQ(parameter_blocks_with_data.front().start_timestamp,
            kStartTimestamp);
  EXPECT_EQ(parameter_blocks_with_data.front().end_timestamp, kEndTimestamp);
}

}  // namespace
}  // namespace iamf_tools
