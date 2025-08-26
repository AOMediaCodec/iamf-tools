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
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_processor.h"
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
#include "iamf/common/read_bit_buffer.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl::MakeConstSpan;

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::iamf_tools_cli_proto::UserMetadata;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kStereoSubstreamId = 999;
constexpr DecodedUleb128 kParameterBlockId = 100;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr int kExpectedPcmBitDepth = 16;
constexpr int16_t kUserProvidedIntegratedLoudness = 0;

constexpr bool kNoRedundantCopy = false;
constexpr bool kRedundantCopy = true;
constexpr bool kInvalidatesBitstream = true;
constexpr bool kDoesNotInvalidateBitstream = false;

constexpr auto kExpectedPrimaryProfile = ProfileVersion::kIamfSimpleProfile;

const auto kOmitOutputWavFiles =
    RenderingMixPresentationFinalizer::ProduceNoSampleProcessors;

constexpr std::array<InternalSampleType, 8> kZeroSamples = {0.0, 0.0, 0.0, 0.0,
                                                            0.0, 0.0, 0.0, 0.0};
// A convenient view when multiple `kZeroSamples` are used in a coupled
// substream.
constexpr std::array<uint8_t, 32> kEightCoupled16BitPcmSamples = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr absl::string_view kArbitraryObuPayload = "\x01\x02\x03\x04";
constexpr std::array<uint8_t, 4> kArbitraryObuPayloadAsUint8 = {0x01, 0x02,
                                                                0x03, 0x04};

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
  auto* audio_element_metadata = user_metadata.add_audio_element_metadata();
  ASSERT_THAT(builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  *audio_element_metadata),
              IsOk());
  audio_element_metadata->set_audio_substream_ids(0, kStereoSubstreamId);
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

void AddArbitraryObuForFirstTick(UserMetadata& user_metadata,
                                 bool invalidates_bitstream) {
  auto* new_arbitrary_obu = user_metadata.add_arbitrary_obu_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_AUDIO_FRAMES_AT_TICK
        insertion_tick: 0
        obu_type: OBU_IA_RESERVED_26
      )pb",
      new_arbitrary_obu));
  new_arbitrary_obu->set_payload(kArbitraryObuPayload);
  new_arbitrary_obu->set_invalidates_bitstream(invalidates_bitstream);
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
        subblocks:
        [ {
          mix_gain_parameter_data {
            animation_type: ANIMATE_STEP
            param_data { step { start_point_value: 0 } }
          }
        }]
      )pb",
      metadata));
  // Configure to be a single subblock.
  metadata->set_parameter_id(kParameterBlockId);
  metadata->set_duration(kNumSamplesPerFrame);
  metadata->set_constant_subblock_duration(kNumSamplesPerFrame);

  // Overwrite `start_timestamp`.
  metadata->set_start_timestamp(start_timestamp);
}

api::IamfTemporalUnitData MakeStereoTemporalUnitData(
    absl::Span<const double> samples) {
  using enum ::iamf_tools_cli_proto::ChannelLabel;
  return api::IamfTemporalUnitData{
      .audio_element_id_to_data = {
          {kAudioElementId,
           {{CHANNEL_LABEL_L_2, samples}, {CHANNEL_LABEL_R_2, samples}}}}};
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
    auto iamf_encoder =
        IamfEncoder::Create(user_metadata_, renderer_factory_.get(),
                            loudness_calculator_factory_.get(),
                            sample_processor_factory_, obu_sequencer_factory_);
    EXPECT_THAT(iamf_encoder, IsOkAndHolds(NotNull()));
    // Most users can use the `IamfEncoder` on the heap. For convenience, we
    // check that it is valid and unwrap it.
    return std::move(**iamf_encoder);
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
  IamfEncoder::ObuSequencerFactory obu_sequencer_factory_ =
      IamfEncoder::CreateNoObuSequencers;
};

TEST_F(IamfEncoderTest, CreateFailsOnEmptyUserMetadata) {
  user_metadata_.Clear();

  EXPECT_FALSE(IamfEncoder::Create(user_metadata_, renderer_factory_.get(),
                                   loudness_calculator_factory_.get(),
                                   sample_processor_factory_,
                                   obu_sequencer_factory_)
                   .ok());
}

TEST_F(IamfEncoderTest, GetRedundatantDescriptorObusIsUnimplemented) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();
  std::vector<uint8_t> output_obus;
  bool get_descriptors_obus_are_finalized;

  EXPECT_THAT(
      iamf_encoder.GetDescriptorObus(kRedundantCopy, output_obus,
                                     get_descriptors_obus_are_finalized),
      StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_F(IamfEncoderTest, CreateGeneratesDescriptorObus) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();
  // Get the serialized descriptor OBUs.
  std::vector<uint8_t> output_obus;
  bool get_descriptors_obus_are_finalized;
  EXPECT_THAT(
      iamf_encoder.GetDescriptorObus(kNoRedundantCopy, output_obus,
                                     get_descriptors_obus_are_finalized),
      IsOk());

  // Parse them back as a "trivial" IA Sequence.
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(output_obus));
  ASSERT_NE(rb, nullptr);
  bool output_insufficient_data = false;
  auto obu_processor = ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                                            rb.get(), output_insufficient_data);
  ASSERT_NE(obu_processor, nullptr);
  EXPECT_FALSE(output_insufficient_data);
  // Check that the expected OBUs are present.
  EXPECT_FALSE(get_descriptors_obus_are_finalized);
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetPrimaryProfile(),
            kExpectedPrimaryProfile);
  EXPECT_EQ(obu_processor->codec_config_obus_.size(), 1);
  EXPECT_EQ(obu_processor->audio_elements_.size(), 1);
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
  // Also, check the equivalent in the deprecated getters.
  EXPECT_EQ(iamf_encoder.GetAudioElements().size(), 1);
  bool get_mix_presentation_obus_are_finalized = false;
  EXPECT_EQ(iamf_encoder
                .GetMixPresentationObus(get_mix_presentation_obus_are_finalized)
                .size(),
            1);
  EXPECT_FALSE(get_mix_presentation_obus_are_finalized);
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
  AddArbitraryObuForFirstTick(user_metadata_, kDoesNotInvalidateBitstream);
  const ArbitraryObu kExpectedArbitraryObu(
      kObuIaReserved26, ObuHeader(), kArbitraryObuPayloadAsUint8,
      ArbitraryObu::kInsertionHookAfterAudioFramesAtTick, 0,
      kDoesNotInvalidateBitstream);
  auto iamf_encoder = CreateExpectOk();
  // Push the first temporal unit.
  EXPECT_THAT(iamf_encoder.Encode(
                  MakeStereoTemporalUnitData(MakeConstSpan(kZeroSamples))),
              IsOk());
  const AudioFrameObu kExpectedAudioFrame(
      ObuHeader(), kStereoSubstreamId,
      MakeConstSpan(kEightCoupled16BitPcmSamples));
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());

  // Arbitrary OBUs come out based on their insertion hook.
  std::vector<uint8_t> output_obus;
  EXPECT_THAT(iamf_encoder.OutputTemporalUnit(output_obus), IsOk());

  // TODO(b/329705373): Use `CollectObusFromIaSequence()` once Arbitrary OBUs
  //                    can be parsed back.
  // Expect the serialized data serialized the same as the expected OBUs.
  const std::vector<uint8_t> serialized_obus = SerializeObusExpectOk(
      std::list<const ObuBase*>{&kExpectedAudioFrame, &kExpectedArbitraryObu});
  EXPECT_EQ(output_obus, serialized_obus);
}

TEST_F(IamfEncoderTest, OutputTemporalUnitFailsForExtranousArbitraryObus) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddArbitraryObuForFirstTick(user_metadata_, kInvalidatesBitstream);
  auto iamf_encoder = CreateExpectOk();
  // Ok, this is is a trivial IA Sequence.
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());

  // Normally all temporal units must have an audio frame, extraneous arbitrary
  // OBUs may be present and are signalled as if data OBUs are still available.
  // They result in failure, because only the test suite should actually
  // generate extraneous arbitrary OBUs.
  EXPECT_TRUE(iamf_encoder.GeneratingTemporalUnits());

  std::vector<uint8_t> output_obus;
  EXPECT_THAT(iamf_encoder.OutputTemporalUnit(output_obus), Not(IsOk()));
  // TODO(b/278865608): Find a way to test these arbitrary OBUs are serialized
  //                   (for the test suite). The backing sequencer detects they
  //                   are invalid, and aborts before they can be observed.
  EXPECT_TRUE(output_obus.empty());
}

TEST_F(IamfEncoderTest, GenerateDataObusTwoIterationsSucceeds) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  AddParameterBlockAtTimestamp(0, user_metadata_);
  AddParameterBlockAtTimestamp(8, user_metadata_);
  auto iamf_encoder = CreateExpectOk();
  std::vector<uint8_t> output_obus;
  bool unused_output_obus_are_finalized = false;
  EXPECT_THAT(iamf_encoder.GetDescriptorObus(kNoRedundantCopy, output_obus,
                                             unused_output_obus_are_finalized),
              IsOk());
  // Configure a buffer, so we can parse the descriptor and each temporal unit
  // in separate chunks. To examine the raw output is expected.
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  ASSERT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(MakeConstSpan(output_obus)), IsOk());
  bool output_insufficient_data = false;
  auto obu_processor =
      ObuProcessor::Create(true, rb.get(), output_insufficient_data);
  EXPECT_NE(obu_processor, nullptr);
  EXPECT_FALSE(output_insufficient_data);

  // Temporary variables for one iteration.
  std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  int iteration = 0;
  auto temporal_unit_data =
      MakeStereoTemporalUnitData(MakeConstSpan(kZeroSamples));
  while (iamf_encoder.GeneratingTemporalUnits()) {
    temporal_unit_data.parameter_block_id_to_metadata[kParameterBlockId] =
        user_metadata_.parameter_block_metadata(iteration);
    EXPECT_THAT(iamf_encoder.Encode(temporal_unit_data), IsOk());

    // Signal stopping adding samples at the second iteration.
    if (iteration == 1) {
      EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
    }

    // Output.
    EXPECT_THAT(iamf_encoder.OutputTemporalUnit(output_obus), IsOk());
    EXPECT_THAT(rb->PushBytes(MakeConstSpan(output_obus)), IsOk());
    EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                    /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                    /*continue_processing=*/continue_processing),
                IsOk());

    ASSERT_TRUE(output_temporal_unit.has_value());
    EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
    EXPECT_EQ(output_temporal_unit->output_parameter_blocks.size(), 1);
    EXPECT_EQ(output_temporal_unit->output_timestamp,
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
  std::vector<uint8_t> output_obus;
  bool unused_output_obus_are_finalized = false;
  EXPECT_THAT(iamf_encoder.GetDescriptorObus(kNoRedundantCopy, output_obus,
                                             unused_output_obus_are_finalized),
              IsOk());
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  ASSERT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(MakeConstSpan(output_obus)), IsOk());

  // Use many parts of the API, to make sure the move did not break anything.
  EXPECT_TRUE(iamf_encoder.GeneratingTemporalUnits());
  auto temporal_unit_data =
      MakeStereoTemporalUnitData(MakeConstSpan(kZeroSamples));
  temporal_unit_data.parameter_block_id_to_metadata.emplace(
      kParameterBlockId, user_metadata_.parameter_block_metadata(0));
  EXPECT_THAT(iamf_encoder.Encode(temporal_unit_data), IsOk());
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
  EXPECT_THAT(iamf_encoder.OutputTemporalUnit(output_obus), IsOk());
  EXPECT_THAT(rb->PushBytes(MakeConstSpan(output_obus)), IsOk());

  // Collect the full IA Sequence.
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentations;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  EXPECT_THAT(CollectObusFromIaSequence(
                  *rb, ia_sequence_header, codec_config_obus, audio_elements,
                  mix_presentations, audio_frames, parameter_blocks),
              IsOk());
  // Check that the OBUs look reasonable.
  EXPECT_EQ(ia_sequence_header.GetPrimaryProfile(), kExpectedPrimaryProfile);
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_EQ(audio_elements.size(), 1);
  EXPECT_EQ(mix_presentations.size(), 1);
  EXPECT_EQ(audio_frames.size(), 1);
  EXPECT_EQ(parameter_blocks.size(), 1);
}

TEST_F(IamfEncoderTest, CallingFinalizeEncodeTwiceSucceeds) {
  SetupDescriptorObus();
  auto iamf_encoder = CreateExpectOk();
  // The first call is OK.
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());

  // There is nothing to finalize a second time, the calls safely do nothing.
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
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
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
  EXPECT_FALSE(iamf_encoder.GeneratingTemporalUnits());

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
              CreateLoudnessCalculator(_, _, _))
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

TEST_F(IamfEncoderTest, LoudnessIsFinalizedAfterAlignedOrTrivialIaSequence) {
  SetupDescriptorObus();
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();

  // `FinalizeEncode()` may trigger loudness finalization for trivial or
  // frame-aligned IA Sequences.
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());

  EXPECT_FALSE(iamf_encoder.GeneratingTemporalUnits());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, LoudnessIsFinalizedAfterFinalOutputTemporalUnit) {
  SetupDescriptorObus();
  AddAudioFrame(user_metadata_);
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();
  // Make stereo data with a single sample for each channel, to force a
  // non-frame aligned IA sequence.
  constexpr auto kOneSample = MakeConstSpan(kZeroSamples).first(1);
  EXPECT_THAT(iamf_encoder.Encode(MakeStereoTemporalUnitData(kOneSample)),
              IsOk());
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
  // Despite `FinalizeEncode()` being called, there are data OBUs to push
  // out. Loudness is intentionally not yet finalized.
  EXPECT_TRUE(iamf_encoder.GeneratingTemporalUnits());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kUserProvidedIntegratedLoudness);
  EXPECT_FALSE(obus_are_finalized);

  // Outputting the final temporal unit triggers loudness finalization.
  std::vector<uint8_t> unused_output_obus;
  EXPECT_THAT(iamf_encoder.OutputTemporalUnit(unused_output_obus), IsOk());

  EXPECT_FALSE(iamf_encoder.GeneratingTemporalUnits());
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, LoudnessIsFinalizedAfterArbitraryDataObus) {
  SetupDescriptorObus();
  AddArbitraryObuForFirstTick(user_metadata_, kInvalidatesBitstream);
  AddAudioFrame(user_metadata_);
  renderer_factory_ = std::make_unique<RendererFactory>();
  constexpr int16_t kIntegratedLoudness = 999;
  loudness_calculator_factory_ =
      GetLoudnessCalculatorWhichReturnsIntegratedLoudness(kIntegratedLoudness);
  auto iamf_encoder = CreateExpectOk();
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());

  // As a special case, when there are extra "data" arbitrary OBUs. Loudness is
  // not computed until all are generated.
  EXPECT_TRUE(iamf_encoder.GeneratingTemporalUnits());
  bool obus_are_finalized = false;
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kUserProvidedIntegratedLoudness);
  EXPECT_FALSE(obus_are_finalized);

  // Outputting the final temporal unit triggers loudness finalization.
  std::vector<uint8_t> unused_output_obus;
  // The last temporal unit is invalid, because there is an extraneous arbitrary
  // OBU. Regardless, loudness is finalized.
  EXPECT_THAT(iamf_encoder.OutputTemporalUnit(unused_output_obus), Not(IsOk()));

  // After the last data OBUs are generated, loudness is finalized.
  EXPECT_FALSE(iamf_encoder.GeneratingTemporalUnits());
  ExpectFirstLayoutIntegratedLoudnessIs(
      iamf_encoder.GetMixPresentationObus(obus_are_finalized),
      kIntegratedLoudness);
  EXPECT_TRUE(obus_are_finalized);
}

TEST_F(IamfEncoderTest, GetDescriptorObusHasFilledInLoudness) {
  SetupDescriptorObus();
  // Loudness measurement is done only when the signal can be rendered, and
  // based on the resultant loudness calculators.
  renderer_factory_ = std::make_unique<RendererFactory>();
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  const LoudnessInfo kArbitraryLoudnessInfo = {
      .info_type = 0,
      .integrated_loudness = 123,
      .digital_peak = 456,
  };
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  auto iamf_encoder = CreateExpectOk();
  EXPECT_THAT(iamf_encoder.FinalizeEncode(), IsOk());
  EXPECT_FALSE(iamf_encoder.GeneratingTemporalUnits());

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
