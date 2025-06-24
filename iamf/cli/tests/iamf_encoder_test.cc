#include "iamf/cli/iamf_encoder.h"

#include <array>
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
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
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
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl::MakeConstSpan;

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::iamf_tools_cli_proto::UserMetadata;
using ::testing::_;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr int kExpectedPcmBitDepth = 16;
constexpr int16_t kUserProvidedIntegratedLoudness = 0;

constexpr auto kExpectedPrimaryProfile = ProfileVersion::kIamfSimpleProfile;

const auto kOmitOutputWavFiles =
    RenderingMixPresentationFinalizer::ProduceNoSampleProcessors;

constexpr std::array<InternalSampleType, 8> kZeroSamples = {0.0, 0.0, 0.0, 0.0,
                                                            0.0, 0.0, 0.0, 0.0};

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
            sample_rate: 16000
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
  auto* new_mix_presentation = user_metadata.add_mix_presentation_metadata();
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
      new_mix_presentation));
  new_mix_presentation->mutable_sub_mixes(0)
      ->mutable_layouts(0)
      ->mutable_loudness()
      ->set_integrated_loudness(kUserProvidedIntegratedLoudness);
}

void AddDescriptorArbitraryObu(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_AUDIO_ELEMENTS
        obu_type: OBU_IA_RESERVED_26
        payload: "Imaginary descriptor OBU between the audio element and mix presentation."
      )pb",
      user_metadata.add_arbitrary_obu_metadata()));
}

void AddArbitraryObuForFirstTick(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_AUDIO_FRAMES_AT_TICK
        insertion_tick: 0
        obu_type: OBU_IA_RESERVED_26
        payload: "Imaginary temporal unit OBU in the first temporal unit."
      )pb",
      user_metadata.add_arbitrary_obu_metadata()));
}

void AddAudioFrame(UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        samples_to_trim_at_end: 0
        samples_to_trim_at_start: 0
        samples_to_trim_at_end_includes_padding: false
        samples_to_trim_at_start_includes_codec_delay: false
        audio_element_id: 300
        channel_ids: [ 0, 1 ]
        channel_labels: [ "L2", "R2" ]
      )pb",
      user_metadata.add_audio_frame_metadata()));
}

void AddParameterBlockAtTimestamp(InternalTimestamp start_timestamp,
                                  UserMetadata& user_metadata) {
  auto* metadata = user_metadata.add_parameter_block_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
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
        loudness_calculator_factory_.get(), sample_processor_factory_);
    EXPECT_THAT(iamf_encoder, IsOk());
    return std::move(*iamf_encoder);
  }

  UserMetadata user_metadata_;
  // Default some dependencies to be based on the real `IamfComponents`
  // implementations. And generally disable wav writing since it is not needed
  // for most tests.
  std::unique_ptr<RendererFactoryBase> renderer_factory_ =
      CreateRendererFactory();
  std::unique_ptr<LoudnessCalculatorFactoryBase> loudness_calculator_factory_ =
      CreateLoudnessCalculatorFactory();
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory_ = kOmitOutputWavFiles;
};

TEST_F(IamfEncoderTest, CreateFailsOnEmptyUserMetadata) {
  user_metadata_.Clear();

  EXPECT_FALSE(IamfEncoder::Create(user_metadata_, renderer_factory_.get(),
                                   loudness_calculator_factory_.get(),
                                   sample_processor_factory_)
                   .ok());
}

TEST_F(IamfEncoderTest, CreateGeneratesDescriptorObus) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();

  EXPECT_EQ(iamf_encoder.GetIaSequenceHeaderObu().GetPrimaryProfile(),
            kExpectedPrimaryProfile);
  EXPECT_EQ(iamf_encoder.GetCodecConfigObus().size(), 1);
  EXPECT_EQ(iamf_encoder.GetAudioElements().size(), 1);
  bool obus_are_finalized = false;
  EXPECT_EQ(iamf_encoder.GetMixPresentationObus(obus_are_finalized).size(), 1);
  EXPECT_FALSE(obus_are_finalized);
  EXPECT_TRUE(iamf_encoder.GetDescriptorArbitraryObus().empty());
}

TEST_F(IamfEncoderTest, CreateGeneratesDescriptorArbitraryObus) {
  SetupDescriptorObus();
  AddDescriptorArbitraryObu(user_metadata_);

  auto iamf_encoder = CreateExpectOk();

  EXPECT_EQ(iamf_encoder.GetDescriptorArbitraryObus().size(), 1);
}

TEST_F(IamfEncoderTest, BuildInformationTagIsPresentByDefault) {
  SetupDescriptorObus();

  auto iamf_encoder = CreateExpectOk();
  bool unused_obus_are_finalized = false;
  const auto& mix_presentation_obus =
      iamf_encoder.GetMixPresentationObus(unused_obus_are_finalized);
  ASSERT_FALSE(mix_presentation_obus.empty());

  // We don't care which slot the build information tag is in. But we want it to
  // be present by default, to help with debugging.
  const auto& first_obu_tags =
      mix_presentation_obus.front().mix_presentation_tags_;
  ASSERT_TRUE(first_obu_tags.has_value());
  EXPECT_THAT(first_obu_tags->tags, Contains(TagMatchesBuildInformation()));
}

TEST_F(IamfEncoderTest,
       OutputTemporalUnitReturnsArbitraryObusBasedOnInsertionTick) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddArbitraryObuForFirstTick(user_metadata_);
  auto iamf_encoder = CreateExpectOk();
  // Push the first temporal unit.
  iamf_encoder.BeginTemporalUnit();
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2,
                          MakeConstSpan(kZeroSamples));
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2,
                          MakeConstSpan(kZeroSamples));
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());

  // Arbitrary OBUs come out based on their insertion hook.
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  std::list<ArbitraryObu> temp_arbitrary_obus;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(temp_audio_frames, temp_parameter_blocks,
                                      temp_arbitrary_obus),
      IsOk());

  EXPECT_EQ(temp_audio_frames.size(), 1);
  EXPECT_EQ(temp_arbitrary_obus.size(), 1);
}

TEST_F(IamfEncoderTest,
       OutputTemporalUnitMayOutputExtranousArbitraryObusAfterFinalizing) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddArbitraryObuForFirstTick(user_metadata_);
  auto iamf_encoder = CreateExpectOk();
  // Ok, this is is a trivial IA Sequence.
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
  iamf_encoder.BeginTemporalUnit();

  // Normally all temporal units must have an audio frame, but extraneous
  // arbitrary OBUs are allowed, are signalled as if data OBUs are still
  // available.
  EXPECT_TRUE(iamf_encoder.GeneratingDataObus());
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  std::list<ArbitraryObu> temp_arbitrary_obus;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(temp_audio_frames, temp_parameter_blocks,
                                      temp_arbitrary_obus),
      IsOk());

  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());
  EXPECT_EQ(temp_arbitrary_obus.size(), 1);
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
  std::list<ArbitraryObu> temp_arbitrary_obus;
  IdLabeledFrameMap id_to_labeled_frame;
  int iteration = 0;
  while (iamf_encoder.GeneratingDataObus()) {
    iamf_encoder.BeginTemporalUnit();
    iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2,
                            MakeConstSpan(zero_samples));
    iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2,
                            MakeConstSpan(zero_samples));

    // Signal stopping adding samples at the second iteration.
    if (iteration == 1) {
      EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
    }

    EXPECT_THAT(iamf_encoder.AddParameterBlockMetadata(
                    user_metadata_.parameter_block_metadata(iteration)),
                IsOk());

    // Output.
    EXPECT_THAT(
        iamf_encoder.OutputTemporalUnit(
            temp_audio_frames, temp_parameter_blocks, temp_arbitrary_obus),
        IsOk());
    EXPECT_EQ(temp_audio_frames.size(), 1);
    EXPECT_EQ(temp_parameter_blocks.size(), 1);
    EXPECT_EQ(temp_audio_frames.front().start_timestamp,
              iteration * kNumSamplesPerFrame);
    EXPECT_TRUE(temp_arbitrary_obus.empty());

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
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2,
                          MakeConstSpan(kZeroSamples));
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2,
                          MakeConstSpan(kZeroSamples));
  EXPECT_THAT(iamf_encoder.AddParameterBlockMetadata(
                  user_metadata_.parameter_block_metadata(0)),
              IsOk());
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  std::list<ArbitraryObu> temp_arbitrary_obus;
  IdLabeledFrameMap id_to_labeled_frame;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(temp_audio_frames, temp_parameter_blocks,
                                      temp_arbitrary_obus),
      IsOk());
  EXPECT_EQ(temp_audio_frames.size(), 1);
  EXPECT_EQ(temp_parameter_blocks.size(), 1);
  EXPECT_TRUE(temp_arbitrary_obus.empty());
}

TEST_F(IamfEncoderTest, CallingFinalizeAddSamplesTwiceSucceeds) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();
  // The first call is OK.
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());

  // There is nothing to finalize a second time, the calls safely do nothing.
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
}

TEST_F(
    IamfEncoderTest,
    GetMixPresentationMaintainsOriginalLoudnessWhenLoudnessCalculatorIsDisabled) {
  SetupDescriptorObus();
  // Configuring the encoder with null factories is permitted, which disables
  // rendering and loudness measurements.
  renderer_factory_ = nullptr;
  loudness_calculator_factory_ = nullptr;
  auto iamf_encoder = CreateExpectOk();
  bool obus_are_finalized = false;
  const auto original_loudness =
      iamf_encoder.GetMixPresentationObus(obus_are_finalized)
          .front()
          .sub_mixes_.front()
          .layouts.front()
          .loudness;
  EXPECT_FALSE(obus_are_finalized);
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());

  EXPECT_EQ(iamf_encoder.GetMixPresentationObus(obus_are_finalized)
                .front()
                .sub_mixes_.front()
                .layouts.front()
                .loudness,
            original_loudness);
  EXPECT_TRUE(obus_are_finalized);
}

// Returns a mock loudness calculator factory that results in calculating the
// given integrated loudness when queried.
std::unique_ptr<LoudnessCalculatorFactoryBase>
GetLoudnessCalculatorWhichReturnsIntegratedLoudness(
    int16_t integrated_loudness) {
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  const LoudnessInfo kArbitraryLoudnessInfo = {
      .info_type = 0,
      .integrated_loudness = integrated_loudness,
      .digital_peak = 0,
  };
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  return mock_loudness_calculator_factory;
}

void ExpectFirstLayoutIntegratedLoudnessIs(
    const std::list<MixPresentationObu>& mix_presentation_obus,
    int16_t expected_integrated_loudness) {
  ASSERT_FALSE(mix_presentation_obus.empty());
  EXPECT_EQ(mix_presentation_obus.front()
                .sub_mixes_.front()
                .layouts.front()
                .loudness.integrated_loudness,
            expected_integrated_loudness);
}

TEST_F(IamfEncoderTest, LoudessIsFinalizedAfterAlignedOrTrivialIaSequence) {
  SetupDescriptorObus();
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();

  // `FinalizeAddSamples()` may trigger loudness finalization for trivial or
  // frame-aligned IA Sequences.
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());

  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, LoudessIsFinalizedAfterFinalOutputTemporalUnit) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();
  iamf_encoder.BeginTemporalUnit();
  // Add in a single sample for each channel, to result in a non-frame aligned
  // IA sequence.
  constexpr auto kOneSample = absl::MakeConstSpan(kZeroSamples).first(1);
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kL2, kOneSample);
  iamf_encoder.AddSamples(kAudioElementId, ChannelLabel::kR2, kOneSample);
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
  // Despite `FinalizeAddSamples()` being called, there are data OBUs to push
  // out. Loudness is intentionally not yet finalized.
  EXPECT_TRUE(iamf_encoder.GeneratingDataObus());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kUserProvidedIntegratedLoudness);
  EXPECT_FALSE(obus_are_finalized);

  // Outputting the final temporal unit triggers loudness finalization.
  std::list<AudioFrameWithData> unused_audio_frames;
  std::list<ParameterBlockWithData> unused_parameter_blocks;
  std::list<ArbitraryObu> unused_arbitrary_obus;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(
          unused_audio_frames, unused_parameter_blocks, unused_arbitrary_obus),
      IsOk());

  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, LoudessIsFinalizedAfterArbitraryDataObus) {
  SetupDescriptorObus();
  AddArbitraryObuForFirstTick(user_metadata_);
  AddAudioFrame(user_metadata_);
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());

  // As a special case, when there are extra "data" arbitrary OBUs. Loudness is
  // not computed until all are generated.
  EXPECT_TRUE(iamf_encoder.GeneratingDataObus());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kUserProvidedIntegratedLoudness);
  EXPECT_FALSE(obus_are_finalized);

  // Outputting the final temporal unit triggers loudness finalization.
  std::list<AudioFrameWithData> temp_audio_frames;
  std::list<ParameterBlockWithData> temp_parameter_blocks;
  std::list<ArbitraryObu> temp_arbitrary_obus;
  EXPECT_THAT(
      iamf_encoder.OutputTemporalUnit(temp_audio_frames, temp_parameter_blocks,
                                      temp_arbitrary_obus),
      IsOk());

  // After the last data OBUs are generated, loudness is finalized.
  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, GetMixPresentationObusHasFilledInLoudness) {
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
  EXPECT_THAT(iamf_encoder.FinalizeAddSamples(), IsOk());
  EXPECT_FALSE(iamf_encoder.GeneratingDataObus());

  bool obus_are_finalized = false;
  const auto& finalized_mix_presentation_obus =
      iamf_encoder.GetMixPresentationObus(obus_are_finalized);
  EXPECT_TRUE(obus_are_finalized);
  ASSERT_FALSE(finalized_mix_presentation_obus.empty());

  EXPECT_EQ(finalized_mix_presentation_obus.front()
                .sub_mixes_.front()
                .layouts.front()
                .loudness,
            kArbitraryLoudnessInfo);
}

TEST_F(IamfEncoderTest, OutputSampleProcessorFactoryIgnoresBitDepthOverride) {
  // The override bit-depth should be used at the `SampleProcessorFactory`
  // level.
  SetupDescriptorObus();
  constexpr uint32_t kExpectedSampleProcessorFactoryCalledBitDepth =
      kExpectedPcmBitDepth;
  constexpr uint32_t kIgnoredBitDepthOverride = 255;
  user_metadata_.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(kIgnoredBitDepthOverride);
  // Wav file writing is done only when the signal can be rendered, based on the
  // resultant wav writers.
  renderer_factory_ = std::make_unique<RendererFactory>();
  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(
      mock_sample_processor_factory,
      Call(_, _, _, _, _, _, kExpectedSampleProcessorFactoryCalledBitDepth, _));
  sample_processor_factory_ = mock_sample_processor_factory.AsStdFunction();

  CreateExpectOk();
}

// TODO(b/349321277): Add more tests.

}  // namespace
}  // namespace iamf_tools
