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
#include "iamf/obu/mix_presentation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/bit_buffer_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/tests/obu_test_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

// Bit shifts for the `layout_type` and `sound_system` fields which are stored
// in the same byte.
constexpr int kLayoutTypeBitShift = 6;
constexpr int kSoundSystemBitShift = 2;

class MixPresentationObuTest : public ObuTestBase, public testing::Test {
 public:
  // Used to populate a `MixPresentationSubMix`.
  struct DynamicSubMixArguments {
    // Outer vector has length `num_audio_elements`. Inner has length
    // `num_subblocks`.
    std::vector<std::vector<uint32_t>> element_mix_gain_subblocks;

    // Length `num_subblocks`.
    std::vector<uint32_t> output_mix_gain_subblocks;
  };

  MixPresentationObuTest()
      : ObuTestBase(
            /*expected_header=*/{kObuIaMixPresentation << 3, 47},
            /*expected_payload=*/
            {
                // Start Mix OBU.
                10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1',
                '\0', 1,
                // Start Submix 1
                1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
                // Start RenderingConfig.
                RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
                // End RenderingConfig.
                12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
                // Start Layout 1 (of Submix 1).
                (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
                    LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
                LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
                // End Mix OBU.
            }),
        mix_presentation_id_(10),
        count_label_(1),
        annotations_language_({"en-us"}),
        localized_presentation_annotations_({"Mix 1"}),
        num_sub_mixes_(1),
        sub_mixes_(
            {{.num_audio_elements = 1,
              .audio_elements = {{
                  .audio_element_id = 11,
                  .localized_element_annotations = {"Submix 1"},
                  .rendering_config =
                      {.headphones_rendering_mode =
                           RenderingConfig::kHeadphonesRenderingModeStereo,
                       .reserved = 0,
                       .rendering_config_extension_size = 0,
                       .rendering_config_extension_bytes = {}},
              }},
              .num_layouts = 1,
              .layouts = {{.loudness_layout =
                               {.layout_type =
                                    Layout::kLayoutTypeLoudspeakersSsConvention,
                                .specific_layout =
                                    LoudspeakersSsConventionLayout{
                                        .sound_system =
                                            LoudspeakersSsConventionLayout::
                                                kSoundSystemA_0_2_0,
                                        .reserved = 0}},
                           .loudness = {.info_type = LoudnessInfo::kTruePeak,
                                        .integrated_loudness = 18,
                                        .digital_peak = 19,
                                        .true_peak = 20}}}}}),
        dynamic_sub_mix_args_({{.element_mix_gain_subblocks = {{}},
                                .output_mix_gain_subblocks = {}}}),
        mix_presentation_tags_(std::nullopt) {
    MixGainParamDefinition element_mix_gain;
    element_mix_gain.parameter_id_ = 12;
    element_mix_gain.parameter_rate_ = 13;
    element_mix_gain.param_definition_mode_ = true;
    element_mix_gain.reserved_ = 0;
    element_mix_gain.default_mix_gain_ = 14;
    sub_mixes_[0].audio_elements[0].element_mix_gain = element_mix_gain;

    MixGainParamDefinition output_mix_gain;
    output_mix_gain.parameter_id_ = 15;
    output_mix_gain.parameter_rate_ = 16;
    output_mix_gain.param_definition_mode_ = true;
    output_mix_gain.reserved_ = 0;
    output_mix_gain.default_mix_gain_ = 17;
    sub_mixes_[0].output_mix_gain = output_mix_gain;
  }

  ~MixPresentationObuTest() override = default;

 protected:
  void InitExpectOk() override { InitMixPresentationObu(); }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<MixPresentationObu> obu_;

  DecodedUleb128 mix_presentation_id_;
  DecodedUleb128 count_label_;
  std::vector<std::string> annotations_language_;
  // Length `count_label`.
  std::vector<std::string> localized_presentation_annotations_;

  DecodedUleb128 num_sub_mixes_;
  // Length `num_sub_mixes`.
  std::vector<MixPresentationSubMix> sub_mixes_;

  // Length `num_sub_mixes`.
  std::vector<DynamicSubMixArguments> dynamic_sub_mix_args_;

  std::optional<MixPresentationTags> mix_presentation_tags_;

 private:
  // Copies over data into the output argument `obu->sub_mix[i]`.
  void InitSubMixDynamicMemory(int i) {
    // The sub-mix to initialize.
    auto& sub_mix = sub_mixes_[i];
    const auto& sub_mix_args = dynamic_sub_mix_args_[i];

    // Create and initialize memory for the subblock durations within each
    // audio element.
    ASSERT_EQ(sub_mix_args.element_mix_gain_subblocks.size(),
              sub_mix.audio_elements.size());
    for (int j = 0; j < sub_mix.audio_elements.size(); ++j) {
      auto& audio_element = sub_mix.audio_elements[j];
      const auto& audio_element_args =
          sub_mix_args.element_mix_gain_subblocks[j];

      audio_element.element_mix_gain.InitializeSubblockDurations(
          static_cast<DecodedUleb128>(audio_element_args.size()));
      ASSERT_EQ(audio_element_args.size(),
                audio_element.element_mix_gain.GetNumSubblocks());
      for (int k = 0; k < audio_element_args.size(); ++k) {
        EXPECT_THAT(audio_element.element_mix_gain.SetSubblockDuration(
                        k, audio_element_args[k]),
                    IsOk());
      }
    }

    // Create and initialize memory for the subblock durations within the
    // output mix config.

    sub_mix.output_mix_gain.InitializeSubblockDurations(
        static_cast<DecodedUleb128>(
            sub_mix_args.output_mix_gain_subblocks.size()));
    ASSERT_EQ(sub_mix_args.output_mix_gain_subblocks.size(),
              sub_mix.output_mix_gain.GetNumSubblocks());
    for (int j = 0; j < sub_mix_args.output_mix_gain_subblocks.size(); ++j) {
      EXPECT_THAT(sub_mix.output_mix_gain.SetSubblockDuration(
                      j, sub_mix_args.output_mix_gain_subblocks[j]),
                  IsOk());
    }
  }

  // Copies over data into the output argument `obu`.
  void InitMixPresentationObu() {
    // Allocate and initialize some dynamic memory within `sub_mixes`.
    ASSERT_EQ(dynamic_sub_mix_args_.size(), sub_mixes_.size());
    for (int i = 0; i < sub_mixes_.size(); ++i) {
      InitSubMixDynamicMemory(i);
    }

    // Construct and transfer ownership of the memory to the OBU.
    obu_ = std::make_unique<MixPresentationObu>(
        header_, mix_presentation_id_, count_label_, annotations_language_,
        localized_presentation_annotations_, num_sub_mixes_, sub_mixes_);
  }
};

TEST_F(MixPresentationObuTest, ConstructorSetsObuTyoe) {
  InitExpectOk();

  EXPECT_EQ(obu_->header_.obu_type, kObuIaMixPresentation);
}

TEST_F(MixPresentationObuTest, DefaultSingleStereo) { InitAndTestWrite(); }

TEST_F(MixPresentationObuTest, RedundantCopy) {
  header_.obu_redundant_copy = true;

  expected_header_ = {kObuIaMixPresentation << 3 | kObuRedundantCopyBitMask,
                      47},

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithIllegalTrimmingStatusFlag) {
  header_.obu_trimming_status_flag = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, ExtensionHeader) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaMixPresentation << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      53,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, ValidateAndWriteFailsWithInvalidNumSubMixes) {
  num_sub_mixes_ = 0;
  sub_mixes_.clear();
  dynamic_sub_mix_args_.clear();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithInconsistentCountLabelAndAnnotationsLanguage) {
  count_label_ = 1;
  annotations_language_.clear();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);

  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(
    MixPresentationObuTest,
    ValidateAndWriteFailsWithInconsistentCountLabelAndLocalizedPresentationAnnotations) {
  count_label_ = 1;
  localized_presentation_annotations_.clear();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);

  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(
    MixPresentationObuTest,
    ValidateAndWriteFailsWithInconsistentCountLabelAndLocalizedElementAnnotations) {
  count_label_ = 1;
  sub_mixes_[0].audio_elements[0].localized_element_annotations.clear();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);

  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithInvalidNonUniqueAudioElementIds) {
  ASSERT_EQ(sub_mixes_.size(), 1);
  ASSERT_EQ(sub_mixes_[0].audio_elements.size(), 1);
  // Add an extra audio element to sub-mix.
  sub_mixes_[0].num_audio_elements = 2;
  sub_mixes_[0].audio_elements.push_back(sub_mixes_[0].audio_elements[0]);
  dynamic_sub_mix_args_[0].element_mix_gain_subblocks = {{}, {}};

  // It is forbidden to have duplicate audio element IDs within a mix
  // presentation OBU.
  ASSERT_EQ(sub_mixes_[0].audio_elements[0].audio_element_id,
            sub_mixes_[0].audio_elements[1].audio_element_id);

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithInvalidWhenSubMixHasNoAudioElements) {
  ASSERT_EQ(sub_mixes_.size(), 1);
  // Reconfigure the sub-mix to have no audio elements and no `element_mix`
  // gains which are typically 1:1 with the audio elements.
  sub_mixes_[0].num_audio_elements = 0;
  sub_mixes_[0].audio_elements.clear();
  dynamic_sub_mix_args_[0].element_mix_gain_subblocks.clear();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, TwoAnchorElements) {
  sub_mixes_[0].layouts[0].loudness.info_type = LoudnessInfo::kAnchoredLoudness;
  sub_mixes_[0].layouts[0].loudness.anchored_loudness = {
      2,
      {{.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
        .anchored_loudness = 20},
       {.anchor_element = AnchoredLoudnessElement::kAnchorElementDialogue,
        .anchored_loudness = 21}}};

  expected_header_ = {kObuIaMixPresentation << 3, 52};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1.
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kAnchoredLoudness, 0, 18, 0, 19,
      // Start anchored loudness.
      2, AnchoredLoudnessElement::kAnchorElementAlbum, 0, 20,
      AnchoredLoudnessElement::kAnchorElementDialogue, 0, 21,
      // End anchored loudness.
      // End Layout 1 (of Submix 1).
      // End Submix 1.
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, AnchoredAndTruePeak) {
  sub_mixes_[0].layouts[0].loudness.info_type =
      LoudnessInfo::kAnchoredLoudness | LoudnessInfo::kTruePeak;
  sub_mixes_[0].layouts[0].loudness.true_peak = 22;
  sub_mixes_[0].layouts[0].loudness.anchored_loudness = {
      1,
      {{.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
        .anchored_loudness = 20}}};

  expected_header_ = {kObuIaMixPresentation << 3, 51};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kAnchoredLoudness | LoudnessInfo::kTruePeak, 0, 18, 0, 19,
      // Start true peak.
      0, 22,
      // End true peak.
      // Start anchored loudness.
      1, AnchoredLoudnessElement::kAnchorElementAlbum, 0, 20,
      // End anchored loudness.
      // End Layout 1 (of Submix 1).
      // End Submix 1.
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithInvalidNonUniqueAnchorElement) {
  sub_mixes_[0].layouts[0].loudness.info_type = LoudnessInfo::kAnchoredLoudness;
  sub_mixes_[0].layouts[0].loudness.anchored_loudness = {
      2,
      {{.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
        .anchored_loudness = 20},
       {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
        .anchored_loudness = 21}}};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, ExtensionLayoutZero) {
  sub_mixes_[0].layouts[0].loudness.info_type = 0x04;
  sub_mixes_[0].layouts[0].loudness.layout_extension = {.info_type_size = 0,
                                                        .info_type_bytes{}};

  expected_header_ = {kObuIaMixPresentation << 3, 46};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      0x04, 0, 18, 0, 19, 0,
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  // Initialize a test has several `DecodedUleb128` explicitly in the bitstream.
  sub_mixes_[0].layouts[0].loudness.info_type = 0x04;
  sub_mixes_[0].layouts[0].loudness.layout_extension = {.info_type_size = 0,
                                                        .info_type_bytes{}};

  sub_mixes_[0].audio_elements[0].rendering_config = {
      .headphones_rendering_mode =
          RenderingConfig::kHeadphonesRenderingModeStereo,
      .reserved = 0,
      .rendering_config_extension_size = 2,
      .rendering_config_extension_bytes = {'e', 'x'}};

  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);

  expected_header_ = {kObuIaMixPresentation << 3,
                      // `obu_size` is affected by the `LebGenerator`.
                      0x80 | 60, 0x00};
  expected_payload_ = {
      // Start Mix OBU.
      // `mix_presentation_id` is affected by the `LebGenerator`.
      0x80 | 10, 0x00,
      // `count_label` is affected by the `LebGenerator`.
      0x80 | 1, 0x00,
      // `language_label` and `mix_presentation_annotations`.
      'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0',
      // `num_submixes` is affected by the `LebGenerator`.
      0x80 | 1, 0x00,
      // Start Submix 1
      // `num_audio_elements` is affected by the `LebGenerator`.
      0x80 | 1, 0x00,
      // `audio_element_id` is affected by the `LebGenerator`.
      0x80 | 11, 0x00, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_size` is affected by the `LebGenerator`.
      0x80 | 2, 0x00, 'e', 'x',
      // End RenderingConfig.
      // `element_mix_gain.parameter_id` is affected by the `LebGenerator`.
      0x80 | 12, 0x00,
      // `element_mix_gain.parameter_rate` is affected by the `LebGenerator`.
      0x80 | 13, 0x00, 0x80, 0, 14,
      // `output_mix_gain.parameter_id` is affected by the `LebGenerator`.
      0x80 | 15, 0x00,
      // `output_mix_gain.parameter_rate` is affected by the `LebGenerator`.
      0x80 | 16, 0x00, 0x80, 0, 17,
      // `num_layouts` is affected by the `LebGenerator`.
      0x80 | 1, 0x00,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      0x04, 0, 18, 0, 19,
      // `info_type_size` is affected by the `LebGenerator`.
      0x80 | 0, 0x00
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, ExtensionLayoutNonZero) {
  sub_mixes_[0].layouts[0].loudness.info_type = 0x04;
  sub_mixes_[0].layouts[0].loudness.layout_extension = {
      .info_type_size = 5, .info_type_bytes{'e', 'x', 't', 'r', 'a'}};

  expected_header_ = {kObuIaMixPresentation << 3, 51};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      0x04, 0, 18, 0, 19, 5, 'e', 'x', 't', 'r', 'a',
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithIllegalIamfStringOver128Bytes) {
  // Create a string that has no null terminator in the first 128 bytes.
  const std::string kIllegalIamfString(kIamfMaxStringSize, 'a');
  localized_presentation_annotations_[0] = kIllegalIamfString;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, MultipleLabels) {
  count_label_ = 2;
  annotations_language_ = {"en-us", "en-gb"};
  localized_presentation_annotations_ = {"Mix 1", "Mix 1"};
  sub_mixes_[0].audio_elements[0].localized_element_annotations = {
      "Submix 1", "GB Submix 1"};

  expected_header_ = {kObuIaMixPresentation << 3, 71};
  expected_payload_ = {
      // Start Mix OBU.
      10, 2, 'e', 'n', '-', 'u', 's', '\0', 'e', 'n', '-', 'g', 'b', '\0', 'M',
      'i', 'x', ' ', '1', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0', 'G', 'B', ' ', 'S',
      'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteSucceedsWhenAnnotationsLanguagesAreUnique) {
  const std::vector<std::string> kAnnotationsLanaguesWithDifferentRegions = {
      {"en-us"}, {"en-gb"}};
  annotations_language_ = kAnnotationsLanaguesWithDifferentRegions;

  count_label_ = 2;
  localized_presentation_annotations_ = {"0", "1"};
  sub_mixes_[0].audio_elements[0].localized_element_annotations = {"0", "1"};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu_->ValidateAndWriteObu(unused_wb), IsOk());
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWhenAnnotationsLanguagesAreNotUnique) {
  const std::vector<std::string> kInvalidAnnotationsLanguagesWithDuplicate = {
      {"en-us"}, {"en-us"}};
  annotations_language_ = kInvalidAnnotationsLanguagesWithDuplicate;

  // Configure plausible values for the related fields.
  count_label_ = 2;
  localized_presentation_annotations_ = {"0", "1"};
  sub_mixes_[0].audio_elements[0].localized_element_annotations = {"0", "1"};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, BinauralRenderingConfig) {
  sub_mixes_[0].audio_elements[0].rendering_config = {
      .headphones_rendering_mode =
          RenderingConfig::kHeadphonesRenderingModeStereo,
      .reserved = 0,
      .rendering_config_extension_size = 0,
      .rendering_config_extension_bytes = {}};

  expected_header_ = {kObuIaMixPresentation << 3, 47};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1.
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(
    MixPresentationObuTest,
    ValidateAndWriteFailsWithOverflowBinauralRenderingConfigReservedOverSixBits) {
  sub_mixes_[0].audio_elements[0].rendering_config = {
      .headphones_rendering_mode =
          RenderingConfig::kHeadphonesRenderingModeStereo,
      .reserved = (1 << 6),
      .rendering_config_extension_size = 0,
      .rendering_config_extension_bytes = {}};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest,
       ValidateAndWriteFailsWithOverflowSsLayoutReservedOverTwoBits) {
  std::get<LoudspeakersSsConventionLayout>(
      sub_mixes_[0].layouts[0].loudness_layout.specific_layout)
      .reserved = (1 << 2);

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, RenderingConfigExtension) {
  sub_mixes_[0].audio_elements[0].rendering_config = {
      .headphones_rendering_mode =
          RenderingConfig::kHeadphonesRenderingModeStereo,
      .reserved = 0,
      .rendering_config_extension_size = 2,
      .rendering_config_extension_bytes = {'e', 'x'}};

  expected_header_ = {kObuIaMixPresentation << 3, 49};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 2, 'e', 'x',
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, MultipleSubmixesAndLayouts) {
  num_sub_mixes_ = 2;
  sub_mixes_.push_back(
      {.num_audio_elements = 1,
       .audio_elements = {{
           .audio_element_id = 21,
           .localized_element_annotations = {"Submix 2"},
           .rendering_config =
               {.headphones_rendering_mode =
                    RenderingConfig::kHeadphonesRenderingModeBinaural,
                .reserved = 0,
                .rendering_config_extension_size = 0,
                .rendering_config_extension_bytes = {}},
       }},

       .num_layouts = 3,
       .layouts = {
           {.loudness_layout = {.layout_type = Layout::kLayoutTypeReserved0,
                                .specific_layout =
                                    LoudspeakersReservedOrBinauralLayout{
                                        .reserved = 0}},
            .loudness = {.info_type = LoudnessInfo::kTruePeak,
                         .integrated_loudness = 28,
                         .digital_peak = 29,
                         .true_peak = 30}},

           {.loudness_layout =
                {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
                 .specific_layout =
                     LoudspeakersSsConventionLayout{
                         .sound_system = LoudspeakersSsConventionLayout::
                             kSoundSystemA_0_2_0,
                         .reserved = 0}},
            .loudness = {.info_type = 0,
                         .integrated_loudness = 31,
                         .digital_peak = 32,
                         .true_peak = 0}},

           {.loudness_layout = {.layout_type = Layout::kLayoutTypeBinaural,
                                .specific_layout =
                                    LoudspeakersReservedOrBinauralLayout{
                                        .reserved = 0}},
            .loudness = {.info_type = LoudnessInfo::kTruePeak,
                         .integrated_loudness = 34,
                         .digital_peak = 35,
                         .true_peak = 36}},
       }});
  MixGainParamDefinition element_mix_gain;
  element_mix_gain.parameter_id_ = 22;
  element_mix_gain.parameter_rate_ = 23;
  element_mix_gain.param_definition_mode_ = true;
  element_mix_gain.reserved_ = 0;
  element_mix_gain.default_mix_gain_ = 24;
  sub_mixes_.back().audio_elements[0].element_mix_gain = element_mix_gain;

  MixGainParamDefinition output_mix_gain;
  output_mix_gain.parameter_id_ = 25;
  output_mix_gain.parameter_rate_ = 26;
  output_mix_gain.param_definition_mode_ = true;
  output_mix_gain.reserved_ = 0;
  output_mix_gain.default_mix_gain_ = 27;
  sub_mixes_.back().output_mix_gain = output_mix_gain;

  dynamic_sub_mix_args_.push_back(
      {.element_mix_gain_subblocks = {{}}, .output_mix_gain_subblocks = {}});

  expected_header_ = {kObuIaMixPresentation << 3, 93};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 2,
      // Start Submix 1.
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
      // Start Submix 2.
      1, 21, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '2', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeBinaural << 6, 0,
      // End RenderingConfig.
      22, 23, 0x80, 0, 24, 25, 26, 0x80, 0, 27, 3,
      // Start Layout1 (Submix 2).
      Layout::kLayoutTypeReserved0 << 6, LoudnessInfo::kTruePeak, 0, 28, 0, 29,
      0, 30,
      // Start Layout2 (Submix 2).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      0, 0, 31, 0, 32,
      // Start Layout3 (Submix 2).
      (Layout::kLayoutTypeBinaural << 6), LoudnessInfo::kTruePeak, 0, 34, 0, 35,
      0, 36
      // End Mix OBU.
  };

  InitAndTestWrite();
}

TEST_F(MixPresentationObuTest, ValidateAndWriteFailsWithInvalidMissingStero) {
  sub_mixes_[0].layouts[0].loudness_layout = {
      .layout_type = Layout::kLayoutTypeBinaural,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixPresentationObuTest, WritesMixPresentionTags) {
  expected_header_ = {kObuIaMixPresentation << 3, 58};
  expected_payload_ = {
      // Start Mix OBU.
      10, 1, 'e', 'n', '-', 'u', 's', '\0', 'M', 'i', 'x', ' ', '1', '\0', 1,
      // Start Submix 1
      1, 11, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      12, 13, 0x80, 0, 14, 15, 16, 0x80, 0, 17, 1,
      // Start Layout 1 (of Submix 1).
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kTruePeak, 0, 18, 0, 19, 0, 20,
      // Start Mix Presentation Tags.
      // `num_tags`.
      1,
      // `tag_name[0]`.
      't', 'a', 'g', '\0',
      // `tag_value[0]`.
      'v', 'a', 'l', 'u', 'e', '\0',
      // End Mix OBU.
  };
  InitExpectOk();
  obu_->mix_presentation_tags_ =
      MixPresentationTags{.num_tags = 1, .tags = {{"tag", "value"}}};

  WriteBitBuffer wb(1024);
  EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, expected_header_, expected_payload_);
}

class GetNumChannelsFromLayoutTest : public testing::Test {
 public:
  GetNumChannelsFromLayoutTest()
      : layout_({.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
                 .specific_layout = LoudspeakersSsConventionLayout{
                     .sound_system =
                         LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0,
                     .reserved = 0}}) {}

 protected:
  Layout layout_;
};

TEST_F(GetNumChannelsFromLayoutTest, SoundSystemMono) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0;
  const int32_t kExpectedMonoChannels = 1;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpectedMonoChannels);
}

TEST_F(GetNumChannelsFromLayoutTest, SoundSystemStereo) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::
      LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0;
  const int32_t kExpectedStereoChannels = 2;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpectedStereoChannels);
}

TEST_F(GetNumChannelsFromLayoutTest, SoundSystem5_1) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0;
  const int32_t kExpected5_1Channels = 6;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpected5_1Channels);
}

TEST_F(GetNumChannelsFromLayoutTest, SoundSystem7_1_4) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0;
  const int32_t kExpected7_1_4Channels = 12;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpected7_1_4Channels);
}

TEST_F(GetNumChannelsFromLayoutTest, SoundSystem9_1_6) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0;
  const int32_t kExpected9_1_6Channels = 16;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpected9_1_6Channels);
}

TEST_F(GetNumChannelsFromLayoutTest, LayoutTypeBinaural) {
  layout_ = {
      .layout_type = Layout::kLayoutTypeBinaural,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};
  const int32_t kExpectedBinauralChannels = 2;

  int32_t num_channels;
  EXPECT_THAT(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, num_channels),
      IsOk());
  EXPECT_EQ(num_channels, kExpectedBinauralChannels);
}

TEST_F(GetNumChannelsFromLayoutTest, UnsupportedReservedLayoutType) {
  layout_ = {
      .layout_type = Layout::kLayoutTypeReserved0,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

  int32_t unused_num_channels;
  EXPECT_FALSE(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, unused_num_channels)
          .ok());
}

TEST_F(GetNumChannelsFromLayoutTest, UnsupportedReservedSoundSystem) {
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystemBeginReserved;

  int32_t unused_num_channels;
  EXPECT_FALSE(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, unused_num_channels)
          .ok());
}

TEST_F(MixPresentationObuTest, ValidateAndWriteFailsWithErrorBeyondLayoutType) {
  // `Layout::LayoutType` is 2-bit enum in IAMF. It is invalid for the value to
  // be out of range.
  const auto kBeyondLayoutType = static_cast<Layout::LayoutType>(4);
  // Since a stereo layout must be present, add a new layout and configure
  // `num_layouts` correctly.
  ASSERT_FALSE(sub_mixes_.empty());
  sub_mixes_[0].layouts.push_back(
      MixPresentationLayout{Layout{.layout_type = kBeyondLayoutType}});
  sub_mixes_[0].num_layouts = sub_mixes_[0].layouts.size();

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(GetNumChannelsFromLayoutTest, ErrorBeyondReservedSoundSystem) {
  // `LoudspeakersSsConventionLayout::SoundSystem` is a 4-bit enum in the spec.
  // It is invalid for the value to be out of this range.
  const auto kBeyondSoundSystemReserved =
      static_cast<LoudspeakersSsConventionLayout::SoundSystem>(16);
  std::get<LoudspeakersSsConventionLayout>(layout_.specific_layout)
      .sound_system = kBeyondSoundSystemReserved;

  int32_t unused_num_channels;
  EXPECT_FALSE(
      MixPresentationObu::GetNumChannelsFromLayout(layout_, unused_num_channels)
          .ok());
}

// --- Begin CreateFromBuffer tests ---
TEST(CreateFromBufferTest, RejectEmptyBitstream) {
  std::vector<uint8_t> source;
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  EXPECT_FALSE(
      MixPresentationObu::CreateFromBuffer(header, source.size(), buffer).ok());
}

TEST(CreateFromBuffer, InvalidWithNoSubMixes) {
  std::vector<uint8_t> source = {
      // Start Mix OBU.
      // mix_presentation_id
      10,
      // count_label
      1,
      // annotations_language[0]
      'e', 'n', '-', 'u', 's', '\0',
      // localized_presentation_annotations[0]
      'M', 'i', 'x', ' ', '1', '\0',
      // num_submixes
      0,
      // End Mix OBU.
  };
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  EXPECT_FALSE(
      MixPresentationObu::CreateFromBuffer(header, source.size(), buffer).ok());
}

TEST(CreateFromBuffer, ReadsOneSubMix) {
  const std::vector<std::string> kAnnotationsLanguage = {"en-us"};
  const std::vector<std::string> kLocalizedPresentationAnnotations = {"Mix 1"};
  const std::vector<std::string> kAudioElementLocalizedElementAnnotations = {
      "Submix 1"};

  std::vector<uint8_t> source = {
      // Start Mix OBU.
      // mix_presentation_id
      10,
      // count_label
      1,
      // annotations_language[0]
      'e', 'n', '-', 'u', 's', '\0',
      // localized_presentation_annotations[0]
      'M', 'i', 'x', ' ', '1', '\0',
      // num_submixes
      1,
      // Start Submix.
      1, 21,
      // localized_element_annotations[0]
      'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      22, 23, 0x80, 0, 24, 25, 26, 0x80, 0, 27,
      // num_layouts
      2,
      // Start Layout1.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0 << 2),
      0, 0, 31, 0, 32,
      // Start Layout2.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      0, 0, 31, 0, 32,
      // End SubMix.
      // End Mix OBU.
  };
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  auto obu =
      MixPresentationObu::CreateFromBuffer(header, source.size(), buffer);
  ASSERT_THAT(obu, IsOk());
  EXPECT_EQ(obu->header_.obu_type, kObuIaMixPresentation);
  EXPECT_EQ(obu->GetMixPresentationId(), 10);
  EXPECT_EQ(obu->GetAnnotationsLanguage(), kAnnotationsLanguage);
  EXPECT_EQ(obu->GetLocalizedPresentationAnnotations(),
            kLocalizedPresentationAnnotations);
  EXPECT_EQ(obu->GetNumSubMixes(), 1);
  ASSERT_FALSE(obu->sub_mixes_[0].audio_elements.empty());
  EXPECT_EQ(obu->sub_mixes_[0].audio_elements[0].localized_element_annotations,
            kAudioElementLocalizedElementAnnotations);
}

TEST(CreateFromBufferTest, ReadsMixPresentationTagsIntoFooter) {
  const std::vector<uint8_t> kMixPresentationTags = {
      // Start MixPresentationTags.
      1,
      // Start Tag1.
      'A', 'B', 'C', '\0', '1', '2', '3', '\0',
      // End Tag1.
  };
  std::vector<uint8_t> source = {
      // Start Mix OBU.
      // mix_presentation_id
      10,
      // count_label
      0,
      // num_submixes
      1,
      // Start Submix.
      1, 21,
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      22, 23, 0x80, 0, 24, 25, 26, 0x80, 0, 27,
      // num_layouts
      1,
      // Start Layout0.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      0, 0, 31, 0, 32,
      // End SubMix.
  };
  source.insert(source.end(), kMixPresentationTags.begin(),
                kMixPresentationTags.end());
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  auto obu =
      MixPresentationObu::CreateFromBuffer(header, source.size(), buffer);
  ASSERT_THAT(obu, IsOk());

  EXPECT_FALSE(obu->mix_presentation_tags_.has_value());
  EXPECT_EQ(obu->footer_, kMixPresentationTags);
}

TEST(CreateFromBufferTest, SucceedsWithDuplicateContentLanguageTags) {
  const std::vector<uint8_t> kDuplicateContentLanguageTags = {
      // Start MixPresentationTags.
      2,
      // `tag_name[0]`.
      'c', 'o', 'n', 't', 'e', 'n', 't', '_', 'l', 'a', 'n', 'g', 'u', 'a', 'g',
      'e', '\0',
      // `tag_value[0]`.
      'e', 'n', '-', 'u', 's', '\0',
      // `tag_name[1]`.
      'c', 'o', 'n', 't', 'e', 'n', 't', '_', 'l', 'a', 'n', 'g', 'u', 'a', 'g',
      'e', '\0',
      // `tag_value[1]`.
      'e', 'n', '-', 'g', 'b', '\0'};
  std::vector<uint8_t> source = {
      // Start Mix OBU.
      // mix_presentation_id
      10,
      // count_label
      0,
      // num_submixes
      1,
      // Start Submix.
      1, 21,
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      22, 23, 0x80, 0, 24, 25, 26, 0x80, 0, 27,
      // num_layouts
      1,
      // Start Layout0.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      0, 0, 31, 0, 32,
      // End SubMix.
  };
  source.insert(source.end(), kDuplicateContentLanguageTags.begin(),
                kDuplicateContentLanguageTags.end());
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  auto obu =
      MixPresentationObu::CreateFromBuffer(header, source.size(), buffer);
  ASSERT_THAT(obu, IsOk());

  EXPECT_FALSE(obu->mix_presentation_tags_.has_value());
  EXPECT_EQ(obu->footer_, kDuplicateContentLanguageTags);
}

TEST(ReadSubMixAudioElementTest, AllFieldsPresent) {
  std::vector<uint8_t> source = {
      // Start SubMixAudioElement.
      // audio_element_id
      11,
      // localized_element_annotations[0]
      'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeBinaural << 6, 0,
      // End RenderingConfig.
      // Start ElementMixGain
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80,
      // Default Mix Gain.
      0, 4
      // End ElementMixGain
  };
  ReadBitBuffer buffer(1024, &source);
  SubMixAudioElement audio_element;
  EXPECT_THAT(audio_element.ReadAndValidate(/*count_label=*/1, buffer), IsOk());

  // Set up expected values.
  SubMixAudioElement expected_submix_audio_element = SubMixAudioElement{
      .audio_element_id = 11,
      .localized_element_annotations = {"Submix 1"},
      .rendering_config =
          {.headphones_rendering_mode =
               RenderingConfig::kHeadphonesRenderingModeBinaural},
      .element_mix_gain = MixGainParamDefinition()};
  expected_submix_audio_element.element_mix_gain.parameter_id_ = 0;
  expected_submix_audio_element.element_mix_gain.parameter_rate_ = 1;
  expected_submix_audio_element.element_mix_gain.param_definition_mode_ = true;
  expected_submix_audio_element.element_mix_gain.reserved_ = 0;
  expected_submix_audio_element.element_mix_gain.default_mix_gain_ = 4;
  EXPECT_EQ(audio_element, expected_submix_audio_element);
}

// TODO(b/339855295): Add more tests.
TEST(ReadMixPresentationLayoutTest, LoudSpeakerWithAnchoredLoudness) {
  std::vector<uint8_t> source = {
      // Start Layout.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      LoudnessInfo::kAnchoredLoudness, 0, 18, 0, 19,
      // Start anchored loudness.
      2, AnchoredLoudnessElement::kAnchorElementAlbum, 0, 20,
      AnchoredLoudnessElement::kAnchorElementDialogue, 0, 21,
      // End anchored loudness.
      // End Layout.
  };
  ReadBitBuffer buffer(1024, &source);
  MixPresentationLayout layout;
  EXPECT_THAT(layout.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(layout.loudness_layout.layout_type,
            Layout::kLayoutTypeLoudspeakersSsConvention);
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(
                layout.loudness_layout.specific_layout),
            LoudspeakersSsConventionLayout(
                {.sound_system =
                     LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}));
  EXPECT_EQ(layout.loudness.info_type, LoudnessInfo::kAnchoredLoudness);
  EXPECT_EQ(layout.loudness.anchored_loudness.num_anchored_loudness, 2);
  EXPECT_EQ(layout.loudness.anchored_loudness.anchor_elements[0].anchor_element,
            AnchoredLoudnessElement::kAnchorElementAlbum);
  EXPECT_EQ(
      layout.loudness.anchored_loudness.anchor_elements[0].anchored_loudness,
      20);
  EXPECT_EQ(layout.loudness.anchored_loudness.anchor_elements[1].anchor_element,
            AnchoredLoudnessElement::kAnchorElementDialogue);
  EXPECT_EQ(
      layout.loudness.anchored_loudness.anchor_elements[1].anchored_loudness,
      21);
}

TEST(LoudspeakersSsConventionLayoutRead, ReadsSsConventionLayout) {
  // SS Convention layout is only 6-bits. Ensure the data to be read is in the
  // upper 6-bits of the buffer.
  const int kSsConventionBitShift = 2;
  constexpr auto kSoundSystem =
      LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0;
  constexpr uint8_t kArbitraryTwoBitReservedField = 3;
  std::vector<uint8_t> source = {
      (kSoundSystem << kSoundSystemBitShift | kArbitraryTwoBitReservedField)
      << kSsConventionBitShift};
  ReadBitBuffer buffer(1024, &source);
  LoudspeakersSsConventionLayout ss_convention_layout;

  EXPECT_THAT(ss_convention_layout.Read(buffer), IsOk());

  EXPECT_EQ(ss_convention_layout.sound_system, kSoundSystem);
  EXPECT_EQ(ss_convention_layout.reserved, kArbitraryTwoBitReservedField);
}

TEST(LoudspeakersReservedOrBinauralLayoutRead, ReadsReservedField) {
  // Binaural layout is only 6-bits. Ensure the data to be read is in the
  // upper 6-bits of the buffer.
  const int kBinauralLayoutBitShift = 2;
  constexpr uint8_t kArbitrarySixBitReservedField = 63;
  std::vector<uint8_t> source = {kArbitrarySixBitReservedField
                                 << kBinauralLayoutBitShift};
  ReadBitBuffer buffer(1024, &source);
  LoudspeakersReservedOrBinauralLayout reserved_binaural_layout;

  EXPECT_THAT(reserved_binaural_layout.Read(buffer), IsOk());

  EXPECT_EQ(reserved_binaural_layout.reserved, kArbitrarySixBitReservedField);
}

TEST(LayoutReadAndValidate, ReadsLoudspeakersSsConventionLayout) {
  constexpr auto kSoundSystem =
      LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0;
  constexpr uint8_t kArbitraryTwoBitReservedField = 3;
  std::vector<uint8_t> source = {
      (Layout::kLayoutTypeLoudspeakersSsConvention << kLayoutTypeBitShift) |
      (kSoundSystem << kSoundSystemBitShift | kArbitraryTwoBitReservedField)};
  ReadBitBuffer buffer(1024, &source);
  Layout loudness_layout;

  EXPECT_THAT(loudness_layout.ReadAndValidate(buffer), IsOk());

  EXPECT_EQ(loudness_layout.layout_type,
            Layout::kLayoutTypeLoudspeakersSsConvention);
  ASSERT_TRUE(std::holds_alternative<LoudspeakersSsConventionLayout>(
      loudness_layout.specific_layout));
  const auto& ss_convention_layout =
      std::get<LoudspeakersSsConventionLayout>(loudness_layout.specific_layout);
  EXPECT_EQ(ss_convention_layout.sound_system, kSoundSystem);
  EXPECT_EQ(ss_convention_layout.reserved, kArbitraryTwoBitReservedField);
}

TEST(LayoutReadAndValidate, ReadsReservedLayout) {
  const auto kReservedLayout = Layout::kLayoutTypeReserved0;
  constexpr uint8_t kArbitrarySixBitReservedField = 63;
  std::vector<uint8_t> source = {(kReservedLayout << kLayoutTypeBitShift) |
                                 (kArbitrarySixBitReservedField)};
  ReadBitBuffer buffer(1024, &source);
  Layout loudness_layout;

  EXPECT_THAT(loudness_layout.ReadAndValidate(buffer), IsOk());

  EXPECT_EQ(loudness_layout.layout_type, kReservedLayout);
  ASSERT_TRUE(std::holds_alternative<LoudspeakersReservedOrBinauralLayout>(
      loudness_layout.specific_layout));
  EXPECT_EQ(std::get<LoudspeakersReservedOrBinauralLayout>(
                loudness_layout.specific_layout)
                .reserved,
            kArbitrarySixBitReservedField);
}

TEST(LayoutReadAndValidate, ReadsBinauralLayout) {
  const auto kBinauralLayout = Layout::kLayoutTypeBinaural;
  constexpr uint8_t kArbitrarySixBitReservedField = 33;
  std::vector<uint8_t> source = {(kBinauralLayout << kLayoutTypeBitShift) |
                                 (kArbitrarySixBitReservedField)};
  ReadBitBuffer buffer(1024, &source);
  Layout loudness_layout;

  EXPECT_THAT(loudness_layout.ReadAndValidate(buffer), IsOk());

  EXPECT_EQ(loudness_layout.layout_type, kBinauralLayout);
  ASSERT_TRUE(std::holds_alternative<LoudspeakersReservedOrBinauralLayout>(
      loudness_layout.specific_layout));
  EXPECT_EQ(std::get<LoudspeakersReservedOrBinauralLayout>(
                loudness_layout.specific_layout)
                .reserved,
            kArbitrarySixBitReservedField);
}

TEST(ReadMixPresentationSubMixTest, AudioElementAndMultipleLayouts) {
  std::vector<uint8_t> source = {
      // Start Submix.
      1, 21, 'S', 'u', 'b', 'm', 'i', 'x', ' ', '1', '\0',
      // Start RenderingConfig.
      RenderingConfig::kHeadphonesRenderingModeStereo << 6, 0,
      // End RenderingConfig.
      22, 23, 0x80, 0, 24, 25, 26, 0x80, 0, 27,
      // num_layouts
      2,
      // Start Layout1.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0 << 2),
      0, 0, 31, 0, 32,
      // Start Layout2.
      (Layout::kLayoutTypeLoudspeakersSsConvention << 6) |
          (LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0 << 2),
      0, 0, 31, 0, 32,
      // End SubMix.
  };
  ReadBitBuffer buffer(1024, &source);
  MixPresentationSubMix sub_mix;
  EXPECT_THAT(sub_mix.ReadAndValidate(/*count_label=*/1, buffer), IsOk());
  EXPECT_EQ(sub_mix.audio_elements.size(), 1);
  EXPECT_EQ(sub_mix.layouts[0].loudness_layout.layout_type,
            Layout::kLayoutTypeLoudspeakersSsConvention);
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(
                sub_mix.layouts[0].loudness_layout.specific_layout),
            LoudspeakersSsConventionLayout(
                {.sound_system =
                     LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0}));
  EXPECT_EQ(sub_mix.layouts[1].loudness_layout.layout_type,
            Layout::kLayoutTypeLoudspeakersSsConvention);
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(
                sub_mix.layouts[1].loudness_layout.specific_layout),
            LoudspeakersSsConventionLayout(
                {.sound_system =
                     LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}));
}

TEST(MixPresentationTagsWriteAndValidate, WritesWithZeroTags) {
  constexpr uint8_t kZeroNumTags = 0;
  const MixPresentationTags kMixPresentationTagsWithZeroTags = {
      .num_tags = kZeroNumTags};
  const std::vector<uint8_t> kExpectedBuffer = {
      // `num_tags`.
      kZeroNumTags,
  };
  WriteBitBuffer wb(1024);

  EXPECT_THAT(kMixPresentationTagsWithZeroTags.ValidateAndWrite(wb), IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBuffer);
}

TEST(MixPresentationTagsWriteAndValidate, WritesContentLanguageTag) {
  constexpr uint8_t kOneTag = 1;
  const MixPresentationTags kMixPresentationTagsWithContentLanguageTag = {
      .num_tags = kOneTag, .tags = {{"content_language", "eng"}}};
  const std::vector<uint8_t> kExpectedBuffer = {
      // `num_tags`.
      kOneTag,
      // `tag_name[0]`.
      'c', 'o', 'n', 't', 'e', 'n', 't', '_', 'l', 'a', 'n', 'g', 'u', 'a', 'g',
      'e', '\0',
      // `tag_value[0]`.
      'e', 'n', 'g', '\0'};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(kMixPresentationTagsWithContentLanguageTag.ValidateAndWrite(wb),
              IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBuffer);
}

TEST(MixPresentationTagsWriteAndValidate,
     InvalidWhenContentLanguageTagNotThreeCharacters) {
  constexpr uint8_t kOneTag = 1;
  const MixPresentationTags kMixPresentationTagsWithContentLanguageTag = {
      .num_tags = kOneTag, .tags = {{"content_language", "en-us"}}};

  WriteBitBuffer wb(0);

  EXPECT_FALSE(
      kMixPresentationTagsWithContentLanguageTag.ValidateAndWrite(wb).ok());
}

TEST(MixPresentationTagsWriteAndValidate, WritesArbitraryTags) {
  constexpr uint8_t kNumTags = 1;
  const MixPresentationTags kMixPresentationTagsWithArbitraryTag = {
      .num_tags = kNumTags, .tags = {{"ABC", "123"}}};
  const std::vector<uint8_t> kExpectedBuffer = {// `num_tags`.
                                                kNumTags,
                                                // `tag_name[0]`.
                                                'A', 'B', 'C', '\0',
                                                // `tag_value[1]`.
                                                '1', '2', '3', '\0'};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(kMixPresentationTagsWithArbitraryTag.ValidateAndWrite(wb),
              IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBuffer);
}

TEST(MixPresentationTagsWriteAndValidate, WritesDuplicateArbitraryTags) {
  constexpr uint8_t kTwoTags = 2;
  const MixPresentationTags kMixPresentationTagsWithArbitraryTag = {
      .num_tags = kTwoTags, .tags = {{"tag", "value"}, {"tag", "value"}}};
  const std::vector<uint8_t> kExpectedBuffer = {// `num_tags`.
                                                kTwoTags,
                                                // `tag_name[0]`.
                                                't', 'a', 'g', '\0',
                                                // `tag_value[0]`.
                                                'v', 'a', 'l', 'u', 'e', '\0',
                                                // `tag_name[1]`.
                                                't', 'a', 'g', '\0',
                                                // `tag_value[1]`.
                                                'v', 'a', 'l', 'u', 'e', '\0'};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(kMixPresentationTagsWithArbitraryTag.ValidateAndWrite(wb),
              IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBuffer);
}

TEST(MixPresentationTagsWriteAndValidate, InvalidForDuplicateContentIdTag) {
  constexpr uint8_t kTwoTags = 2;
  const MixPresentationTags
      kMixPresentationTagsWithDuplicateContentLanguageTag = {
          .num_tags = kTwoTags,
          .tags = {{"content_language", "eng"}, {"content_language", "kor"}}};

  WriteBitBuffer wb(1024);

  EXPECT_FALSE(
      kMixPresentationTagsWithDuplicateContentLanguageTag.ValidateAndWrite(wb)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
