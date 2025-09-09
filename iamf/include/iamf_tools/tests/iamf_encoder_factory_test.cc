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

#include "iamf/include/iamf_tools/iamf_encoder_factory.h"

#include <cstdint>
#include <string>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::iamf_tools_cli_proto::UserMetadata;
using ::testing::Not;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kStereoSubstreamId = 999;
constexpr uint32_t kBitDepth = 16;

void AddIaSequenceHeader(UserMetadata& user_metadata) {
  auto* ia_sequence_header_metadata =
      user_metadata.add_ia_sequence_header_metadata();
  ia_sequence_header_metadata->set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_SIMPLE);
  ia_sequence_header_metadata->set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE);
}

void AddCodecConfig(UserMetadata& user_metadata) {
  auto* new_codec_config = user_metadata.add_codec_config_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 8
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_rate: 16000
          }
        }
      )pb",
      new_codec_config));
  new_codec_config->mutable_codec_config()
      ->mutable_decoder_config_lpcm()
      ->set_sample_size(kBitDepth);
}

void AddAudioElement(UserMetadata& user_metadata) {
  AudioElementMetadataBuilder builder;
  auto* audio_element_metadata = user_metadata.add_audio_element_metadata();
  ASSERT_THAT(builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  *audio_element_metadata),
              IsOk());
  audio_element_metadata->set_audio_substream_ids(0, kStereoSubstreamId);
}

void AddMixPresentation(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        mix_presentation_id: 42
        count_label: 0
        sub_mixes {
          audio_elements {
            audio_element_id: 300
            rendering_config {
              headphones_rendering_mode: HEADPHONES_RENDERING_MODE_STEREO
            }
            element_mix_gain {
              param_definition {
                parameter_id: 100
                parameter_rate: 16000
                param_definition_mode: 1
                reserved: 0
              }
              default_mix_gain: 0
            }
          }
          output_mix_gain {
            param_definition {
              parameter_id: 100
              parameter_rate: 16000
              param_definition_mode: 1
              reserved: 0
            }
            default_mix_gain: 0
          }
          layouts {
            loudness_layout {
              layout_type: LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION
              ss_layout { sound_system: SOUND_SYSTEM_A_0_2_0 reserved: 0 }
            }
            loudness {
              info_type_bit_masks: []
              digital_peak: 0
            }
          }
        }
      )pb",
      user_metadata.add_mix_presentation_metadata()));
}

TEST(CreateFileGeneratingIamfEncoder, SucceedsWithSimpleConfig) {
  UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  AddAudioElement(user_metadata);
  AddMixPresentation(user_metadata);

  std::string user_metadata_string;
  ASSERT_TRUE(user_metadata.SerializeToString(&user_metadata_string));
  auto iamf_encoder = api::IamfEncoderFactory::CreateFileGeneratingIamfEncoder(
      user_metadata_string, GetAndCleanupOutputFileName("output.iamf"));
  EXPECT_THAT(iamf_encoder, IsOk());
  EXPECT_NE(*iamf_encoder, nullptr);
}

TEST(CreateFileGeneratingIamfEncoder, FailsWithInvalidLebGenerator) {
  UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  AddAudioElement(user_metadata);
  AddMixPresentation(user_metadata);
  // Corrupt the configuration for the leb generator.
  user_metadata.mutable_test_vector_metadata()
      ->mutable_leb_generator()
      ->set_mode(iamf_tools_cli_proto::GENERATE_LEB_INVALID);

  std::string user_metadata_string;
  ASSERT_TRUE(user_metadata.SerializeToString(&user_metadata_string));
  auto iamf_encoder = api::IamfEncoderFactory::CreateFileGeneratingIamfEncoder(
      user_metadata_string, GetAndCleanupOutputFileName("output.iamf"));
  EXPECT_THAT(iamf_encoder, Not(IsOk()));
}

TEST(CreateIamfEncoder, SucceedsWithSimpleConfig) {
  UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  AddAudioElement(user_metadata);
  AddMixPresentation(user_metadata);

  std::string user_metadata_string;
  ASSERT_TRUE(user_metadata.SerializeToString(&user_metadata_string));
  auto iamf_encoder =
      api::IamfEncoderFactory::CreateIamfEncoder(user_metadata_string);
  EXPECT_THAT(iamf_encoder, IsOk());
  EXPECT_NE(*iamf_encoder, nullptr);
}

}  // namespace
}  // namespace iamf_tools
