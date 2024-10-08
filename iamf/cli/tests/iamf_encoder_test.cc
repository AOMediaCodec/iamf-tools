#include "iamf/cli/iamf_encoder.h"

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::iamf_tools_cli_proto::UserMetadata;

constexpr DecodedUleb128 kAudioElementId = 300;
constexpr uint32_t kNumSamplesPerFrame = 8;

void AddIaSequenceHeader(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        primary_profile: PROFILE_VERSION_SIMPLE
        additional_profile: PROFILE_VERSION_BASE
      )pb",
      user_metadata.add_ia_sequence_header_metadata()));
}

void AddCodecConfig(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 8
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_size: 16
            sample_rate: 48000
          }
        }
      )pb",
      user_metadata.add_codec_config_metadata()));
}

void AddAudioElement(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 1
        audio_substream_ids: [ 0 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs:
          [ {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 1
          }]
        }
      )pb",
      user_metadata.add_audio_element_metadata()));
}

void AddMixPresentation(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        mix_presentation_id: 42
        count_label: 0
        num_sub_mixes: 1
        sub_mixes {
          num_audio_elements: 1
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
          num_layouts: 1
          layouts {
            loudness_layout {
              layout_type: LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION
              ss_layout { sound_system: SOUND_SYSTEM_A_0_2_0 reserved: 0 }
            }
            loudness {
              info_type_bit_masks: []
              integrated_loudness: 0
              digital_peak: 0
            }
          }
        }
      )pb",
      user_metadata.add_mix_presentation_metadata()));
}

void AddAudioFrame(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        samples_to_trim_at_end: 0
        samples_to_trim_at_start: 0
        audio_element_id: 300
        channel_ids: [ 0, 1 ]
        channel_labels: [ "L2", "R2" ]
      )pb",
      user_metadata.add_audio_frame_metadata()));
}

void AddParameterBlockAtTimestamp(const int32_t start_timestamp,
                                  UserMetadata& user_metadata) {
  auto* metadata = user_metadata.add_parameter_block_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks:
        [ {
          mix_gain_parameter_data {
            animation_type: ANIMATE_STEP
            param_data { step { start_point_value: 0 } }
          }
        }]
      )pb",
      metadata));

  // Overwrite `start_timestamp`.
  metadata->set_start_timestamp(start_timestamp);
}

TEST(IamfEncoderTest, EmptyUserMetadataFails) {
  UserMetadata user_metadata;
  IamfEncoder iamf_encoder(user_metadata);

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;

  EXPECT_FALSE(iamf_encoder
                   .GenerateDescriptorObus(ia_sequence_header_obu,
                                           codec_config_obus, audio_elements,
                                           mix_presentation_obus)
                   .ok());
}

TEST(IamfEncoderTest, GenerateDescriptorObusSucceeds) {
  UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  AddAudioElement(user_metadata);
  AddMixPresentation(user_metadata);
  IamfEncoder iamf_encoder(user_metadata);

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  EXPECT_THAT(iamf_encoder.GenerateDescriptorObus(
                  ia_sequence_header_obu, codec_config_obus, audio_elements,
                  mix_presentation_obus),
              IsOk());

  EXPECT_TRUE(ia_sequence_header_obu.has_value());
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_EQ(audio_elements.size(), 1);
  EXPECT_EQ(mix_presentation_obus.size(), 1);
}

TEST(IamfEncoderTest, GenerateDataObusTwoIterationsSucceeds) {
  UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  AddAudioElement(user_metadata);
  AddMixPresentation(user_metadata);
  AddAudioFrame(user_metadata);
  AddParameterBlockAtTimestamp(0, user_metadata);
  AddParameterBlockAtTimestamp(8, user_metadata);
  IamfEncoder iamf_encoder(user_metadata);

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  ASSERT_THAT(iamf_encoder.GenerateDescriptorObus(
                  ia_sequence_header_obu, codec_config_obus, audio_elements,
                  mix_presentation_obus),
              IsOk());

  // Temporary variables for one iteration.
  const std::vector<InternalSampleType> zero_samples(kNumSamplesPerFrame, 0.0);
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  IdLabeledFrameMap id_to_labeled_frame;
  int32_t output_timestamp = 0;
  int iteration = 0;
  while (iamf_encoder.GeneratingDataObus()) {
    iamf_encoder.BeginTemporalUnit();
    iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2, zero_samples);
    iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2, zero_samples);

    // Signal stopping adding samples at the second iteration.
    if (iteration == 1) {
      iamf_encoder.FinalizeAddSamples();
    }

    EXPECT_THAT(iamf_encoder.AddParameterBlockMetadata(
                    user_metadata.parameter_block_metadata(iteration)),
                IsOk());

    // Output.
    EXPECT_THAT(iamf_encoder.OutputTemporalUnit(
                    temp_audio_frames, temp_parameter_blocks,
                    id_to_labeled_frame, output_timestamp),
                IsOk());
    EXPECT_EQ(temp_audio_frames.size(), 1);
    EXPECT_EQ(temp_parameter_blocks.size(), 1);
    EXPECT_EQ(output_timestamp, iteration * kNumSamplesPerFrame);

    iteration++;
  }

  EXPECT_EQ(iteration, 2);
}
// TODO(b/349321277): Add more tests.

}  // namespace
}  // namespace iamf_tools
