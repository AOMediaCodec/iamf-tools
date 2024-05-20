/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/mix_presentation_generator.h"

#include <cstdint>
#include <limits>
#include <list>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

// TODO(b/296346506): Add more tests for `MixPresentationGenerator`.

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

class MixPresentationGeneratorTest : public ::testing::Test {
 public:
  void SetUp() override {
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
              element_mix_config {
                mix_gain {
                  param_definition {
                    parameter_id: 999
                    parameter_rate: 16000
                    param_definition_mode: 1
                    reserved: 0
                  }
                  default_mix_gain: 0
                }
              }
            }
            output_mix_config {
              output_mix_gain {
                param_definition {
                  parameter_id: 999
                  parameter_rate: 16000
                  param_definition_mode: 1
                  reserved: 0
                }
                default_mix_gain: 0
              }
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
        mix_presentation_metadata_.Add()));

    AddMixPresentationObuWithAudioElementIds(
        /*mix_presentation_id=*/42, /*audio_element_id=*/300,
        /*common_parameter_id=*/999,
        /*common_parameter_rate=*/16000, expected_obus_);
  }

 protected:
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::MixPresentationObuMetadata>
      mix_presentation_metadata_;
  std::list<MixPresentationObu> generated_obus_;
  std::list<MixPresentationObu> expected_obus_;
};

TEST_F(MixPresentationGeneratorTest, EmptyUserMetadataGeneratesNoObus) {
  MixPresentationGenerator generator(/*mix_presentation_metadata=*/{});
  EXPECT_THAT(generator.Generate(generated_obus_), IsOk());
  EXPECT_EQ(generated_obus_, std::list<MixPresentationObu>());
}

TEST_F(MixPresentationGeneratorTest, SSConventionWithOneStereoAudioElement) {
  MixPresentationGenerator generator(mix_presentation_metadata_);
  EXPECT_THAT(generator.Generate(generated_obus_), IsOk());
  EXPECT_EQ(generated_obus_, expected_obus_);
}

TEST_F(MixPresentationGeneratorTest, SupportsUtf8) {
  const std::string kUtf8FourByteSequenceCode = "\xf0\x9d\x85\x9e\x00)";
  mix_presentation_metadata_.at(0).set_count_label(1);
  mix_presentation_metadata_.at(0)
      .add_mix_presentation_annotations_array()
      ->set_mix_presentation_friendly_label(kUtf8FourByteSequenceCode);

  MixPresentationGenerator generator(mix_presentation_metadata_);
  ASSERT_THAT(generator.Generate(generated_obus_), IsOk());
  ASSERT_FALSE(generated_obus_.back().GetMixPresentationAnnotations().empty());

  EXPECT_EQ(generated_obus_.back()
                .GetMixPresentationAnnotations()[0]
                .mix_presentation_friendly_label,
            kUtf8FourByteSequenceCode);
}

TEST_F(MixPresentationGeneratorTest, InvalidHeadphonesRenderingMode) {
  mix_presentation_metadata_.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_rendering_config()
      ->set_headphones_rendering_mode(
          iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_INVALID);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_FALSE(generator.Generate(generated_obus_).ok());
}

TEST_F(MixPresentationGeneratorTest, InvalidInconsistentNumberOfLayouts) {
  // There is one element in the `layouts` array.
  ASSERT_EQ(mix_presentation_metadata_.at(0).sub_mixes(0).layouts().size(), 1);
  // `num_layouts` is inconsistent with the number of layouts in the array.
  const uint32_t kInconsistentNumLayouts = 2;
  mix_presentation_metadata_.at(0).mutable_sub_mixes(0)->set_num_layouts(
      kInconsistentNumLayouts);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_FALSE(generator.Generate(generated_obus_).ok());
}

TEST_F(MixPresentationGeneratorTest, CopiesUserLoudness) {
  const int16_t kIntegratedLoudness = -100;
  const int16_t kDigitalPeak = -101;
  const int16_t kTruePeak = -102;
  auto* loudness = mix_presentation_metadata_.at(0)
                       .mutable_sub_mixes(0)
                       ->mutable_layouts(0)
                       ->mutable_loudness();
  loudness->add_info_type_bit_masks(
      iamf_tools_cli_proto::LOUDNESS_INFO_TYPE_TRUE_PEAK);
  loudness->set_integrated_loudness(kIntegratedLoudness);
  loudness->set_digital_peak(kDigitalPeak);
  loudness->set_true_peak(kTruePeak);
  expected_obus_.back().sub_mixes_[0].layouts[0].loudness = {
      .info_type = LoudnessInfo::kTruePeak,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .true_peak = kTruePeak};

  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_THAT(generator.Generate(generated_obus_), IsOk());
}

TEST_F(MixPresentationGeneratorTest, InvalidLayoutType) {
  mix_presentation_metadata_.at(0)
      .mutable_sub_mixes(0)
      ->mutable_layouts(0)
      ->mutable_loudness_layout()
      ->set_layout_type(iamf_tools_cli_proto::LAYOUT_TYPE_INVALID);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_FALSE(generator.Generate(generated_obus_).ok());
}

TEST_F(MixPresentationGeneratorTest, ReservedLayoutWithOneStereoAudioElement) {
  // Overwrite the user metadata with a reserved layout.
  auto& input_sub_mix = *mix_presentation_metadata_.at(0).mutable_sub_mixes(0);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        loudness_layout {
          layout_type: LAYOUT_TYPE_RESERVED_1
          reserved_or_binaural_layout { reserved: 0 }
        }
        loudness { info_type_bit_masks: [] }
      )pb",
      input_sub_mix.mutable_layouts(0)));

  // Overwrite the expected OBU with a reserved layout. The actual loudness
  // measurements are not modified by the generator.
  expected_obus_.back().sub_mixes_[0].layouts = {
      {.loudness_layout = {.layout_type = Layout::kLayoutTypeReserved1,
                           .specific_layout =
                               LoudspeakersReservedBinauralLayout{.reserved =
                                                                      0}},
       .loudness = {.info_type = 0}}};

  MixPresentationGenerator generator(mix_presentation_metadata_);
  EXPECT_THAT(generator.Generate(generated_obus_), IsOk());
  EXPECT_EQ(generated_obus_, expected_obus_);
}

TEST(CopyInfoType, Zero) {
  iamf_tools_cli_proto::LoudnessInfo user_loudness_info;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        info_type_bit_masks: []
      )pb",
      &user_loudness_info));

  uint8_t output_info_type;
  EXPECT_THAT(MixPresentationGenerator::CopyInfoType(user_loudness_info,
                                                     output_info_type),
              IsOk());
  EXPECT_EQ(output_info_type, 0);
}

TEST(CopyInfoType, SeveralLoudnessTypes) {
  iamf_tools_cli_proto::LoudnessInfo user_loudness_info;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        # The order of provided flags does not matter.
        info_type_bit_masks: [
          LOUDNESS_INFO_TYPE_RESERVED_64,
          LOUDNESS_INFO_TYPE_TRUE_PEAK,
          LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS
        ]
      )pb",
      &user_loudness_info));

  uint8_t output_info_type;
  EXPECT_THAT(MixPresentationGenerator::CopyInfoType(user_loudness_info,
                                                     output_info_type),
              IsOk());
  EXPECT_EQ(output_info_type, LoudnessInfo::kInfoTypeBitMask64 |
                                  LoudnessInfo::kAnchoredLoudness |
                                  LoudnessInfo::kTruePeak);
}

TEST(CopyInfoType, DeprecatedInfoTypeIsNotSupported) {
  iamf_tools_cli_proto::LoudnessInfo user_loudness_info;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        deprecated_info_type: 2  # Anchored Loudness.
      )pb",
      &user_loudness_info));

  uint8_t unused_output_info_type;
  EXPECT_FALSE(MixPresentationGenerator::CopyInfoType(user_loudness_info,
                                                      unused_output_info_type)
                   .ok());
}

TEST(CopyUserIntegratedLoudnessAndPeaks, WithoutTruePeak) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = 0};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        # `integrated_loudness` and `digital_peak` are always included.
        integrated_loudness: -99 digital_peak: -100
      )pb",
      &user_loudness));

  // Configured expected data. The function only writes to the
  // integrated loudness and peak loudness fields.
  const LoudnessInfo expected_output_loudness = {
      .info_type = 0, .integrated_loudness = -99, .digital_peak = -100};

  EXPECT_THAT(MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                  user_loudness, output_loudness),
              IsOk());
  EXPECT_EQ(output_loudness, expected_output_loudness);
}

TEST(CopyUserIntegratedLoudnessAndPeaks, WithTruePeak) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kTruePeak};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        integrated_loudness: -99
        digital_peak: -100
        # `true_peak` is included when `info_type & kTruePeak == 1`.
        true_peak: -101
      )pb",
      &user_loudness));

  // Configured expected data. The function only writes to the
  // integrated loudness and peak loudness fields.
  const LoudnessInfo expected_output_loudness = {
      .info_type = LoudnessInfo::kTruePeak,
      .integrated_loudness = -99,
      .digital_peak = -100,
      .true_peak = -101};

  EXPECT_THAT(MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                  user_loudness, output_loudness),
              IsOk());
  EXPECT_EQ(output_loudness, expected_output_loudness);
}

TEST(CopyUserIntegratedLoudnessAndPeaks, ValidatesIntegratedLoudness) {
  // Configure valid prerequisites.
  LoudnessInfo output_loudness = {.info_type = 0};
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  user_loudness.set_digital_peak(0);

  // Configure `integrated_loudness` that cannot fit into an `int16_t`.
  user_loudness.set_integrated_loudness(std::numeric_limits<int16_t>::max() +
                                        1);
  EXPECT_FALSE(MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                   user_loudness, output_loudness)
                   .ok());
}

TEST(CopyUserIntegratedLoudnessAndPeaks, ValidatesDigitalPeak) {
  // Configure valid prerequisites.
  LoudnessInfo output_loudness = {.info_type = 0};
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  user_loudness.set_integrated_loudness(0);

  // Configure `digital_peak` that cannot fit into an `int16_t`.
  user_loudness.set_digital_peak(std::numeric_limits<int16_t>::min() - 1);

  EXPECT_FALSE(MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                   user_loudness, output_loudness)
                   .ok());
}

TEST(CopyUserIntegratedLoudnessAndPeaks, ValidatesTruePeak) {
  // Configure valid prerequisites.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kTruePeak};
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  user_loudness.set_integrated_loudness(0);
  user_loudness.set_digital_peak(0);

  // Configure `true_peak` that cannot fit into an `int16_t`.
  user_loudness.set_true_peak(std::numeric_limits<int16_t>::max() + 1);

  EXPECT_FALSE(MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                   user_loudness, output_loudness)
                   .ok());
}

TEST(CopyUserAnchoredLoudness, TwoAnchorElements) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kAnchoredLoudness};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        anchored_loudness {
          num_anchored_loudness: 2
          anchor_elements:
          [ { anchor_element: ANCHOR_TYPE_DIALOGUE anchored_loudness: 1000 }
            , { anchor_element: ANCHOR_TYPE_ALBUM anchored_loudness: 1001 }]
      )pb",
      &user_loudness);

  // Configured expected data. The function only writes to the
  // `AnchoredLoudness`.
  const AnchoredLoudness expected_output_loudness = {
      .num_anchored_loudness = 2,
      .anchor_elements = {
          {.anchor_element = AnchoredLoudnessElement::kAnchorElementDialogue,
           .anchored_loudness = 1000},
          {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
           .anchored_loudness = 1001}}};

  EXPECT_THAT(MixPresentationGenerator::CopyUserAnchoredLoudness(
                  user_loudness, output_loudness),
              IsOk());
  EXPECT_EQ(output_loudness.anchored_loudness, expected_output_loudness);
}

TEST(CopyUserAnchoredLoudness, IllegalUnknownAnchorElementEnum) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kAnchoredLoudness};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        anchored_loudness {
          num_anchored_loudness: 1
          anchor_elements:
          [ { anchor_element: ANCHOR_TYPE_NOT_DEFINED anchored_loudness: 1000 }
      )pb",
      &user_loudness);

  EXPECT_FALSE(MixPresentationGenerator::CopyUserAnchoredLoudness(
                   user_loudness, output_loudness)
                   .ok());
}

TEST(CopyUserLayoutExtension, AllInfoTypeExtensions) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type =
                                      LoudnessInfo::kAnyLayoutExtension};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        info_type_size: 3 info_type_bytes: "abc"
      )pb",
      &user_loudness));

  // Configured expected data. The function only writes to the
  // `LayoutExtension`.
  const LayoutExtension expected_layout_extension = {
      .info_type_size = 3,
      .info_type_bytes = std::vector<uint8_t>({'a', 'b', 'c'})};

  EXPECT_THAT(MixPresentationGenerator::CopyUserLayoutExtension(
                  user_loudness, output_loudness),
              IsOk());
  EXPECT_EQ(output_loudness.layout_extension, expected_layout_extension);
}

TEST(CopyUserLayoutExtension, OneInfoTypeExtension) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kInfoTypeBitMask4};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        info_type_size: 3 info_type_bytes: "abc"
      )pb",
      &user_loudness));

  // Configured expected data. The function only writes to the
  // `LayoutExtension`.
  const LayoutExtension expected_layout_extension = {
      .info_type_size = 3,
      .info_type_bytes = std::vector<uint8_t>({'a', 'b', 'c'})};

  EXPECT_THAT(MixPresentationGenerator::CopyUserLayoutExtension(
                  user_loudness, output_loudness),
              IsOk());
  EXPECT_EQ(output_loudness.layout_extension, expected_layout_extension);
}

}  // namespace
}  // namespace iamf_tools
