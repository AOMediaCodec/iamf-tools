#include "iamf/cli/iamf_encoder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::iamf_tools_cli_proto::UserMetadata;
using ::testing::_;
using ::testing::Return;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr int kExpectedPcmBitDepth = 16;

const auto kOmitOutputWavFiles =
    RenderingMixPresentationFinalizer::ProduceNoWavWriters;

void AddIaSequenceHeader(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        primary_profile: PROFILE_VERSION_SIMPLE
        additional_profile: PROFILE_VERSION_BASE
      )pb",
      user_metadata.add_ia_sequence_header_metadata()));
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
            sample_rate: 48000
          }
        }
      )pb",
      new_codec_config));
  new_codec_config->mutable_codec_config()
      ->mutable_decoder_config_lpcm()
      ->set_sample_size(kExpectedPcmBitDepth);
}

void AddAudioElement(UserMetadata& user_metadata) {
  AudioElementMetadataBuilder builder;
  ASSERT_THAT(builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  *user_metadata.add_audio_element_metadata()),
              IsOk());
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

void AddArbitraryObu(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_AUDIO_ELEMENTS
        obu_type: OBU_IA_RESERVED_26
        payload: "Imaginary descriptor OBU between the audio element and mix presentation."
      )pb",
      user_metadata.add_arbitrary_obu_metadata()));
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

std::string GetFirstSubmixFirstLayoutExpectedPath(
    absl::string_view output_directory) {
  return (std::filesystem::path(output_directory) /
          std::filesystem::path("first_file.wav"))
      .string();
}

auto GetWavWriterFactoryThatProducesFirstSubMixFirstLayout(
    absl::string_view output_directory) {
  const std::string output_wav_path =
      GetFirstSubmixFirstLayoutExpectedPath(output_directory);
  return [output_wav_path](
             DecodedUleb128 mix_presentation_id, int sub_mix_index,
             int layout_index, const Layout&, int num_channels, int sample_rate,
             int bit_depth,
             size_t num_samples_per_frame) -> std::unique_ptr<WavWriter> {
    if (sub_mix_index != 0 || layout_index != 0) {
      return nullptr;
    }

    return WavWriter::Create(output_wav_path, num_channels, sample_rate,
                             bit_depth, num_samples_per_frame);
  };
}

class IamfEncoderTest : public ::testing::Test {
 protected:
  void SetupDescriptorObus() {
    AddIaSequenceHeader(user_metadata_);
    AddCodecConfig(user_metadata_);
    AddAudioElement(user_metadata_);
    AddMixPresentation(user_metadata_);
  }

  IamfEncoder CreateExpectOk() {
    auto iamf_encoder = IamfEncoder::Create(
        user_metadata_, renderer_factory_.get(),
        loudness_calculator_factory_.get(), wav_writer_factory_,
        ia_sequence_header_obu_, codec_config_obus_, audio_elements_,
        mix_presentation_obus_, arbitrary_obus_);
    EXPECT_THAT(iamf_encoder, IsOk());
    return std::move(*iamf_encoder);
  }

  UserMetadata user_metadata_;
  std::optional<IASequenceHeaderObu> ia_sequence_header_obu_;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> mix_presentation_obus_;
  std::list<ArbitraryObu> arbitrary_obus_;
  // Default some dependencies to be based on the real `IamfComponents`
  // implementations. And generally disable wav writing since it is not needed
  // for most tests.
  std::unique_ptr<RendererFactoryBase> renderer_factory_ =
      CreateRendererFactory();
  std::unique_ptr<LoudnessCalculatorFactoryBase> loudness_calculator_factory_ =
      CreateLoudnessCalculatorFactory();
  RenderingMixPresentationFinalizer::WavWriterFactory wav_writer_factory_ =
      kOmitOutputWavFiles;
};

TEST_F(IamfEncoderTest, CreateFailsOnEmptyUserMetadata) {
  user_metadata_.Clear();

  EXPECT_FALSE(IamfEncoder::Create(user_metadata_, renderer_factory_.get(),
                                   loudness_calculator_factory_.get(),
                                   wav_writer_factory_, ia_sequence_header_obu_,
                                   codec_config_obus_, audio_elements_,
                                   mix_presentation_obus_, arbitrary_obus_)
                   .ok());
}

TEST_F(IamfEncoderTest, CreateGeneratesDescriptorObus) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();

  EXPECT_TRUE(ia_sequence_header_obu_.has_value());
  EXPECT_EQ(codec_config_obus_.size(), 1);
  EXPECT_EQ(audio_elements_.size(), 1);
  EXPECT_EQ(mix_presentation_obus_.size(), 1);
  EXPECT_TRUE(arbitrary_obus_.empty());
}

TEST_F(IamfEncoderTest, CreateGeneratesArbitraryObus) {
  SetupDescriptorObus();
  AddArbitraryObu(user_metadata_);

  auto iamf_encoder = CreateExpectOk();

  EXPECT_EQ(arbitrary_obus_.size(), 1);
}

TEST_F(IamfEncoderTest, GenerateDataObusTwoIterationsSucceeds) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddParameterBlockAtTimestamp(0, user_metadata_);
  AddParameterBlockAtTimestamp(8, user_metadata_);
  auto iamf_encoder = CreateExpectOk();

  // Temporary variables for one iteration.
  const std::vector<InternalSampleType> zero_samples(kNumSamplesPerFrame, 0.0);
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  IdLabeledFrameMap id_to_labeled_frame;
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
                    user_metadata_.parameter_block_metadata(iteration)),
                IsOk());

    // Output.
    EXPECT_THAT(iamf_encoder.OutputTemporalUnit(temp_audio_frames,
                                                temp_parameter_blocks),
                IsOk());
    EXPECT_EQ(temp_audio_frames.size(), 1);
    EXPECT_EQ(temp_parameter_blocks.size(), 1);
    EXPECT_EQ(temp_audio_frames.front().start_timestamp,
              iteration * kNumSamplesPerFrame);

    iteration++;
  }

  EXPECT_EQ(iteration, 2);
}

TEST_F(IamfEncoderTest, SafeToUseAfterMove) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddParameterBlockAtTimestamp(0, user_metadata_);
  AddParameterBlockAtTimestamp(8, user_metadata_);
  auto iamf_encoder_to_move_from = CreateExpectOk();

  // Move the encoder, and use it.
  IamfEncoder iamf_encoder = std::move(iamf_encoder_to_move_from);

  // Use many parts of the API, to make sure the move did not break anything.
  EXPECT_TRUE(iamf_encoder.GeneratingDataObus());
  iamf_encoder.BeginTemporalUnit();
  const std::vector<InternalSampleType> kZeroSamples(kNumSamplesPerFrame, 0.0);
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2, kZeroSamples);
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2, kZeroSamples);
  EXPECT_THAT(iamf_encoder.AddParameterBlockMetadata(
                  user_metadata_.parameter_block_metadata(0)),
              IsOk());
  iamf_encoder.FinalizeAddSamples();
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  IdLabeledFrameMap id_to_labeled_frame;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(temp_audio_frames, temp_parameter_blocks),
      IsOk());
  EXPECT_EQ(temp_audio_frames.size(), 1);
  EXPECT_EQ(temp_parameter_blocks.size(), 1);
}

TEST_F(IamfEncoderTest, FinalizeMixPresentationObusSucceeds) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();

  iamf_encoder.FinalizeAddSamples();

  EXPECT_THAT(iamf_encoder.FinalizeMixPresentationObus(mix_presentation_obus_),
              IsOk());
}

TEST_F(IamfEncoderTest,
       FinalizeMixPresentationObusDefaultsToPreservingUserLoudness) {
  SetupDescriptorObus();
  // Configuring the encoder with null factories is permitted, which disables
  // rendering and loudness measurements.
  renderer_factory_ = nullptr;
  loudness_calculator_factory_ = nullptr;
  auto iamf_encoder = CreateExpectOk();
  const auto original_loudness = mix_presentation_obus_.front()
                                     .sub_mixes_.front()
                                     .layouts.front()
                                     .loudness;
  iamf_encoder.FinalizeAddSamples();

  EXPECT_THAT(iamf_encoder.FinalizeMixPresentationObus(mix_presentation_obus_),
              IsOk());

  EXPECT_EQ(mix_presentation_obus_.front()
                .sub_mixes_.front()
                .layouts.front()
                .loudness,
            original_loudness);
}

TEST_F(IamfEncoderTest,
       FinalizeMixPresentationObusFailsBeforeGeneratingDataObusIsFinished) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  auto iamf_encoder = CreateExpectOk();

  // The encoder is still generating data OBUs, so it's not possible to know the
  // final loudness values.
  ASSERT_TRUE(iamf_encoder.GeneratingDataObus());

  EXPECT_FALSE(
      iamf_encoder.FinalizeMixPresentationObus(mix_presentation_obus_).ok());
}

TEST_F(IamfEncoderTest, FinalizeMixPresentationObuFillsInLoudness) {
  SetupDescriptorObus();
  // Loudness measurement is done only when the signal can be rendered, and
  // based on the resultant loudness calculators.
  renderer_factory_ = std::make_unique<RendererFactory>();
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  const LoudnessInfo kArbitraryLoudnessInfo = {
      .info_type = LoudnessInfo::kTruePeak,
      .integrated_loudness = 123,
      .digital_peak = 456,
      .true_peak = 789,
  };
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  auto iamf_encoder = CreateExpectOk();
  iamf_encoder.FinalizeAddSamples();

  EXPECT_THAT(iamf_encoder.FinalizeMixPresentationObus(mix_presentation_obus_),
              IsOk());
  EXPECT_EQ(mix_presentation_obus_.front()
                .sub_mixes_.front()
                .layouts.front()
                .loudness,
            kArbitraryLoudnessInfo);
};

TEST_F(IamfEncoderTest, OutputWavFactoryIsCalledWithOverrideBitDepth) {
  SetupDescriptorObus();
  constexpr uint32_t kExpectedWavFactoryCalledBitDepth = 32;
  user_metadata_.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(
          kExpectedWavFactoryCalledBitDepth);
  // Wav file writing is done only when the signal can be rendered, based on the
  // resultant wav writers.
  renderer_factory_ = std::make_unique<RendererFactory>();
  MockWavWriterFactory mock_wav_writer_factory;
  EXPECT_CALL(mock_wav_writer_factory,
              Call(_, _, _, _, _, _, kExpectedWavFactoryCalledBitDepth, _));
  wav_writer_factory_ = mock_wav_writer_factory.AsStdFunction();

  CreateExpectOk();
};

TEST_F(IamfEncoderTest, OutputWavWriterFactoryIsCalledWithSaneClampedBitDepth) {
  SetupDescriptorObus();
  // The bit-depth is nonsensically large, normally wav files are limited to 32
  // bits per sample
  user_metadata_.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(256);
  constexpr uint32_t kExpectedWavFactoryCalledBitDepth = 32;
  // Wav file writing is done only when the signal can be rendered, based on the
  // resultant wav writers.
  renderer_factory_ = std::make_unique<RendererFactory>();
  MockWavWriterFactory mock_wav_writer_factory;
  EXPECT_CALL(mock_wav_writer_factory,
              Call(_, _, _, _, _, _, kExpectedWavFactoryCalledBitDepth, _));
  wav_writer_factory_ = mock_wav_writer_factory.AsStdFunction();

  CreateExpectOk();
};

// TODO(b/349321277): Add more tests.

}  // namespace
}  // namespace iamf_tools
