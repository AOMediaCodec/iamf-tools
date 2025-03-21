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
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

// TODO(b/296346506): Add more tests for `MixPresentationGenerator`.

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;
using ::testing::Not;
typedef ::google::protobuf::RepeatedPtrField<
    iamf_tools_cli_proto::MixPresentationObuMetadata>
    MixPresentationObuMetadatas;
using enum iamf_tools_cli_proto::HeadPhonesRenderingMode;

constexpr DecodedUleb128 kMixPresentationId = 42;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kCommonParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = 16000;
constexpr bool kParamDefinitionMode = true;
constexpr uint8_t kParamDefinitionReserved = 0;
constexpr int16_t kZeroMixGain = 0;
constexpr int16_t kNonZeroMixGain = 100;
constexpr bool kOmitBuildInformationTag = false;
constexpr bool kAppendBuildInformationTag = true;

void FillMixGainParamDefinition(
    uint32_t parameter_id, int16_t output_mix_gain,
    iamf_tools_cli_proto::MixGainParamDefinition& mix_gain_param_definition) {
  mix_gain_param_definition.mutable_param_definition()->set_parameter_id(
      parameter_id);
  mix_gain_param_definition.mutable_param_definition()->set_parameter_rate(
      kCommonParameterRate);
  mix_gain_param_definition.mutable_param_definition()
      ->set_param_definition_mode(kParamDefinitionMode);
  mix_gain_param_definition.mutable_param_definition()->set_reserved(
      kParamDefinitionReserved);
  mix_gain_param_definition.set_default_mix_gain(output_mix_gain);
}

// Fills `mix_presentation_metadata` with a single submix that contains a single
// stereo audio element.
void FillMixPresentationMetadata(
    iamf_tools_cli_proto::MixPresentationObuMetadata*
        mix_presentation_metadata) {
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
      mix_presentation_metadata));
  // Also fill in some default values for the per-element and per-submix mix
  // gain parameters.
  FillMixGainParamDefinition(kCommonParameterId, kZeroMixGain,
                             *mix_presentation_metadata->mutable_sub_mixes(0)
                                  ->mutable_audio_elements(0)
                                  ->mutable_element_mix_gain());
  FillMixGainParamDefinition(kCommonParameterId, kZeroMixGain,
                             *mix_presentation_metadata->mutable_sub_mixes(0)
                                  ->mutable_output_mix_gain());
}

TEST(Generate, CopiesSoundSystem13_6_9_0) {
  const auto kExpectedSoundSystem =
      LoudspeakersSsConventionLayout::SoundSystem::kSoundSystem13_6_9_0;
  MixPresentationObuMetadatas mix_presentation_metadata = {};
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0)
      .mutable_sub_mixes(0)
      ->mutable_layouts(0)
      ->mutable_loudness_layout()
      ->mutable_ss_layout()
      ->set_sound_system(iamf_tools_cli_proto::SOUND_SYSTEM_13_6_9_0);
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& generated_specific_layout = generated_obus.front()
                                              .sub_mixes_[0]
                                              .layouts[0]
                                              .loudness_layout.specific_layout;
  EXPECT_TRUE(std::holds_alternative<LoudspeakersSsConventionLayout>(
      generated_specific_layout));
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(generated_specific_layout)
                .sound_system,
            kExpectedSoundSystem);
}

TEST(Generate, CopiesReservedHeadphonesRenderingMode2) {
  const auto kExpectedHeadphonesRenderingMode2 =
      RenderingConfig::kHeadphonesRenderingModeReserved2;
  MixPresentationObuMetadatas mix_presentation_metadata = {};
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_rendering_config()
      ->set_headphones_rendering_mode(
          iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_RESERVED_2);
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  EXPECT_EQ(generated_obus.front()
                .sub_mixes_[0]
                .audio_elements[0]
                .rendering_config.headphones_rendering_mode,
            kExpectedHeadphonesRenderingMode2);
}

TEST(Generate, CopiesReservedHeadphonesRenderingMode3) {
  const auto kExpectedHeadphonesRenderingMode3 =
      RenderingConfig::kHeadphonesRenderingModeReserved3;
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_rendering_config()
      ->set_headphones_rendering_mode(HEADPHONES_RENDERING_MODE_RESERVED_3);
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& generated_rendering_config =
      generated_obus.front().sub_mixes_[0].audio_elements[0].rendering_config;
  EXPECT_EQ(generated_rendering_config.headphones_rendering_mode,
            kExpectedHeadphonesRenderingMode3);
  EXPECT_EQ(generated_rendering_config.rendering_config_extension_size, 0);
  EXPECT_TRUE(
      generated_rendering_config.rendering_config_extension_bytes.empty());
}

TEST(Generate, CopiesRenderingConfigExtension) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& first_rendering_config = *mix_presentation_metadata.at(0)
                                      .mutable_sub_mixes(0)
                                      ->mutable_audio_elements(0)
                                      ->mutable_rendering_config();
  first_rendering_config.set_headphones_rendering_mode(
      HEADPHONES_RENDERING_MODE_RESERVED_3);
  first_rendering_config.set_rendering_config_extension_size(5);
  first_rendering_config.set_rendering_config_extension_bytes("extra");
  constexpr std::array<char, 5> kExpectedRenderingConfigExtensionBytes = {
      'e', 'x', 't', 'r', 'a'};

  MixPresentationGenerator generator(mix_presentation_metadata);
  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& generated_rendering_config =
      generated_obus.front().sub_mixes_[0].audio_elements[0].rendering_config;
  EXPECT_EQ(generated_rendering_config.rendering_config_extension_size, 5);
  EXPECT_THAT(generated_rendering_config.rendering_config_extension_bytes,
              ElementsAreArray(kExpectedRenderingConfigExtensionBytes));
}

TEST(Generate, InvalidWhenRenderingConfigExtensionIsMismatched) {
  constexpr size_t kMismatchedSize = 6;
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& first_rendering_config = *mix_presentation_metadata.at(0)
                                      .mutable_sub_mixes(0)
                                      ->mutable_audio_elements(0)
                                      ->mutable_rendering_config();
  first_rendering_config.set_headphones_rendering_mode(
      HEADPHONES_RENDERING_MODE_RESERVED_3);
  first_rendering_config.set_rendering_config_extension_size(kMismatchedSize);
  first_rendering_config.set_rendering_config_extension_bytes("extra");

  MixPresentationGenerator generator(mix_presentation_metadata);
  std::list<MixPresentationObu> undefined_generated_obus;

  EXPECT_FALSE(
      generator.Generate(kAppendBuildInformationTag, undefined_generated_obus)
          .ok());
}

TEST(Generate, CopiesNoAnnotations) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0).set_count_label(0);
  mix_presentation_metadata.at(0).clear_annotations_language();
  mix_presentation_metadata.at(0).clear_localized_presentation_annotations();
  mix_presentation_metadata.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->clear_localized_element_annotations();

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_TRUE(first_obu.GetAnnotationsLanguage().empty());
  EXPECT_TRUE(first_obu.GetLocalizedPresentationAnnotations().empty());
  EXPECT_TRUE(first_obu.sub_mixes_[0]
                  .audio_elements[0]
                  .localized_element_annotations.empty());
}

TEST(Generate, CopiesDeprecatedAnnotations) {
  constexpr int kCountLabel = 2;
  const std::vector<std::string> kAnnotationsLanguage = {"en-us", "en-gb"};
  const std::vector<std::string> kLocalizedPresentationAnnotations = {
      "US Label", "GB Label"};
  const std::vector<std::string> kAudioElementLocalizedElementAnnotations = {
      "US AE Label", "GB AE Label"};
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_count_label(kCountLabel);
  mix_presentation.mutable_language_labels()->Add(kAnnotationsLanguage.begin(),
                                                  kAnnotationsLanguage.end());
  *mix_presentation.mutable_mix_presentation_annotations_array()
       ->Add()
       ->mutable_mix_presentation_friendly_label() =
      kLocalizedPresentationAnnotations[0];
  *mix_presentation.mutable_mix_presentation_annotations_array()
       ->Add()
       ->mutable_mix_presentation_friendly_label() =
      kLocalizedPresentationAnnotations[1];
  auto* first_element_annotations_array =
      mix_presentation.mutable_sub_mixes(0)
          ->mutable_audio_elements(0)
          ->mutable_mix_presentation_element_annotations_array();
  first_element_annotations_array->Add()->set_audio_element_friendly_label(
      kAudioElementLocalizedElementAnnotations[0]);
  first_element_annotations_array->Add()->set_audio_element_friendly_label(
      kAudioElementLocalizedElementAnnotations[1]);

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_EQ(first_obu.GetAnnotationsLanguage(), kAnnotationsLanguage);
  EXPECT_EQ(first_obu.GetLocalizedPresentationAnnotations(),
            kLocalizedPresentationAnnotations);
  EXPECT_EQ(
      first_obu.sub_mixes_[0].audio_elements[0].localized_element_annotations,
      kAudioElementLocalizedElementAnnotations);
}

TEST(Generate, CopiesAnnotations) {
  constexpr int kCountLabel = 2;
  const std::vector<std::string> kAnnotationsLanguage = {"en-us", "en-gb"};
  const std::vector<std::string> kLocalizedPresentationAnnotations = {
      "US Label", "GB Label"};
  const std::vector<std::string> kAudioElementLocalizedElementAnnotations = {
      "US AE Label", "GB AE Label"};
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_count_label(kCountLabel);
  mix_presentation.mutable_annotations_language()->Add(
      kAnnotationsLanguage.begin(), kAnnotationsLanguage.end());
  mix_presentation.mutable_localized_presentation_annotations()->Add(
      kLocalizedPresentationAnnotations.begin(),
      kLocalizedPresentationAnnotations.end());
  mix_presentation.mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_localized_element_annotations()
      ->Add(kAudioElementLocalizedElementAnnotations.begin(),
            kAudioElementLocalizedElementAnnotations.end());

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_EQ(first_obu.GetAnnotationsLanguage(), kAnnotationsLanguage);
  EXPECT_EQ(first_obu.GetLocalizedPresentationAnnotations(),
            kLocalizedPresentationAnnotations);
  EXPECT_EQ(
      first_obu.sub_mixes_[0].audio_elements[0].localized_element_annotations,
      kAudioElementLocalizedElementAnnotations);
}

TEST(Generate, NonDeprecatedAnnotationsTakePrecedence) {
  constexpr int kCountLabel = 1;
  const std::vector<std::string> kDeprecatedAnotations = {"Deprecated"};
  const std::vector<std::string> kAnnotationsLanguage = {"en-us"};
  const std::vector<std::string> kLocalizedPresentationAnnotations = {
      "US Label"};
  const std::vector<std::string> kAudioElementLocalizedElementAnnotations = {
      "US AE Label"};
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_count_label(kCountLabel);
  mix_presentation.mutable_annotations_language()->Add(
      kAnnotationsLanguage.begin(), kAnnotationsLanguage.end());
  mix_presentation.mutable_localized_presentation_annotations()->Add(
      kLocalizedPresentationAnnotations.begin(),
      kLocalizedPresentationAnnotations.end());
  mix_presentation.mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_localized_element_annotations()
      ->Add(kAudioElementLocalizedElementAnnotations.begin(),
            kAudioElementLocalizedElementAnnotations.end());
  mix_presentation.mutable_language_labels()->Add(kDeprecatedAnotations.begin(),
                                                  kDeprecatedAnotations.end());
  *mix_presentation.mutable_mix_presentation_annotations_array()
       ->Add()
       ->mutable_mix_presentation_friendly_label() = kDeprecatedAnotations[0];
  *mix_presentation.mutable_sub_mixes(0)
       ->mutable_audio_elements(0)
       ->mutable_mix_presentation_element_annotations_array()
       ->Add()
       ->mutable_audio_element_friendly_label() = kDeprecatedAnotations[0];

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_EQ(first_obu.GetAnnotationsLanguage(), kAnnotationsLanguage);
  EXPECT_EQ(first_obu.GetLocalizedPresentationAnnotations(),
            kLocalizedPresentationAnnotations);
  EXPECT_EQ(
      first_obu.sub_mixes_[0].audio_elements[0].localized_element_annotations,
      kAudioElementLocalizedElementAnnotations);
}

TEST(Generate, ObeysInconsistentNumberOfLabels) {
  const std::vector<std::string> kAnnotationsLanguage = {"Language 1",
                                                         "Language 2"};
  const std::vector<std::string> kOnlyOneLocalizedPresentationAnnotation = {
      "Localized annotation 1"};
  const std::vector<std::string> kNoAudioElementLocalizedElementAnnotations =
      {};
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_count_label(2);
  mix_presentation.mutable_annotations_language()->Add(
      kAnnotationsLanguage.begin(), kAnnotationsLanguage.end());
  mix_presentation.mutable_localized_presentation_annotations()->Add(
      kOnlyOneLocalizedPresentationAnnotation.begin(),
      kOnlyOneLocalizedPresentationAnnotation.end());

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_EQ(first_obu.GetAnnotationsLanguage(), kAnnotationsLanguage);
  EXPECT_EQ(first_obu.GetLocalizedPresentationAnnotations(),
            kOnlyOneLocalizedPresentationAnnotation);
  EXPECT_EQ(
      first_obu.sub_mixes_[0].audio_elements[0].localized_element_annotations,
      kNoAudioElementLocalizedElementAnnotations);
}

TEST(Generate, CopiesMixPresentationTagsWithZeroTags) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(true);

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  // We must avoid appending the build information tag, to exercise the "zero
  // tags" case.
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  ASSERT_TRUE(first_obu.mix_presentation_tags_.has_value());
  EXPECT_TRUE(first_obu.mix_presentation_tags_->tags.empty());
}

TEST(Generate, IgnoresDeprecatedNumTags) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(true);
  constexpr uint32_t kIncorrectIgnoredNumTags = 1;
  mix_presentation.mutable_mix_presentation_tags()->set_num_tags(
      kIncorrectIgnoredNumTags);

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus),
              IsOk());

  // Ok safely ignore the deprecated `num_tags` field.
  const auto& first_obu = generated_obus.front();
  ASSERT_TRUE(first_obu.mix_presentation_tags_.has_value());
  EXPECT_TRUE(first_obu.mix_presentation_tags_->tags.empty());
}

TEST(Generate, ReturnsErrorIfUserSpecifies256TagsWithoutBuildInformation) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(true);
  constexpr int kMaxNumTags = 255;
  for (int i = 0; i < kMaxNumTags + 1; ++i) {
    auto* tag = mix_presentation.mutable_mix_presentation_tags()->add_tags();
    tag->set_tag_name("tag_name");
    tag->set_tag_value("tag_value");
  }

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> ignored_generated_obus;
  EXPECT_THAT(
      generator.Generate(kOmitBuildInformationTag, ignored_generated_obus),
      Not(IsOk()));
}

TEST(Generate, ReturnsErrorIfUserSpecifies255TagsWithoutBuildInformation) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(true);
  constexpr int kMaxNumTags = 255;
  for (int i = 0; i < kMaxNumTags; ++i) {
    auto* tag = mix_presentation.mutable_mix_presentation_tags()->add_tags();
    tag->set_tag_name("tag_name");
    tag->set_tag_value("tag_value");
  }

  MixPresentationGenerator generator(mix_presentation_metadata);

  // It would be OK to generate 255 tags, but the build information tag pushes
  // the final count over the limit.
  std::list<MixPresentationObu> ignored_generated_obus;
  EXPECT_THAT(
      generator.Generate(kAppendBuildInformationTag, ignored_generated_obus),
      Not(IsOk()));
}

TEST(Generate, CopiesDuplicateContentLanguageTags) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(true);
  mix_presentation.mutable_mix_presentation_tags()->set_num_tags(2);
  auto* first_tag =
      mix_presentation.mutable_mix_presentation_tags()->add_tags();
  first_tag->set_tag_name("content_language");
  first_tag->set_tag_value("eng");
  auto* second_tag =
      mix_presentation.mutable_mix_presentation_tags()->add_tags();
  second_tag->set_tag_name("content_language");
  second_tag->set_tag_value("kor");

  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  ASSERT_TRUE(first_obu.mix_presentation_tags_.has_value());
  ASSERT_EQ(first_obu.mix_presentation_tags_->tags.size(), 2);
  EXPECT_EQ(first_obu.mix_presentation_tags_->tags[0].tag_name,
            "content_language");
  EXPECT_EQ(first_obu.mix_presentation_tags_->tags[0].tag_value, "eng");
  EXPECT_EQ(first_obu.mix_presentation_tags_->tags[1].tag_name,
            "content_language");
  EXPECT_EQ(first_obu.mix_presentation_tags_->tags[1].tag_value, "kor");
}

TEST(
    Generate,
    MixPresentationTagsAreAbsentIncludeMixPresentationTagsAndOmitBuildInformationTagAreFalse) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(false);
  auto* tag = mix_presentation.mutable_mix_presentation_tags()->add_tags();
  tag->set_tag_name("ignored_tag_name");
  tag->set_tag_value("ignored_tag_value");

  MixPresentationGenerator generator(mix_presentation_metadata);

  // To exercise the "absent" tags case, we must avoid appending the build
  // information tag.
  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_obu = generated_obus.front();
  EXPECT_FALSE(first_obu.mix_presentation_tags_.has_value());
}

struct MixPresentationTagsPresenceTestCase {
  bool include_mix_presentation_tags;
  bool append_build_information_tag;

  // `std::nullopt` means that the tags are expected to be absent.
  std::optional<size_t> expected_num_tags;
  bool expect_build_information_tag_to_be_present;
};

class MixPresentationTagsPresenceTest
    : public ::testing::TestWithParam<MixPresentationTagsPresenceTestCase> {};

TEST_P(MixPresentationTagsPresenceTest, MixPresentationTagsArePresentOrAbsent) {
  const auto& test_case = GetParam();
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  auto& mix_presentation = mix_presentation_metadata.at(0);
  mix_presentation.set_include_mix_presentation_tags(
      test_case.include_mix_presentation_tags);
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(test_case.append_build_information_tag,
                                 generated_obus),
              IsOk());
  ASSERT_FALSE(generated_obus.empty());
  const auto& first_obu = generated_obus.front();

  if (test_case.expected_num_tags.has_value()) {
    EXPECT_TRUE(first_obu.mix_presentation_tags_.has_value());
    EXPECT_EQ(first_obu.mix_presentation_tags_->tags.size(),
              *test_case.expected_num_tags);
    // If the tags are present, the last tag may be the build information tag.
    if (test_case.expect_build_information_tag_to_be_present) {
      ASSERT_FALSE(first_obu.mix_presentation_tags_->tags.empty());
      EXPECT_THAT(first_obu.mix_presentation_tags_->tags.back(),
                  TagMatchesBuildInformation());
    }
  } else {
    EXPECT_FALSE(first_obu.mix_presentation_tags_.has_value());
  }
}

// To strictly exercise Simple or Base profile bitstream with no extensions, we
// can disable both the mix presentation tags and the build information tag.
INSTANTIATE_TEST_SUITE_P(
    MixPresentationTagsAreAbsent, MixPresentationTagsPresenceTest,
    testing::ValuesIn<MixPresentationTagsPresenceTestCase>({{
        .include_mix_presentation_tags = false,
        .append_build_information_tag = false,
        .expected_num_tags = std::nullopt,
        .expect_build_information_tag_to_be_present = false,
    }}));

// The spec supports an edge case where the bitstream signals zero tags are
// present. This mode also is useful if a user wants to provide their own tags,
// but disable the build information tag.
INSTANTIATE_TEST_SUITE_P(
    MixPresentationTagsArePresentWithZeroTags, MixPresentationTagsPresenceTest,
    testing::ValuesIn<MixPresentationTagsPresenceTestCase>({{
        .include_mix_presentation_tags = true,
        .append_build_information_tag = false,
        .expected_num_tags = 0,
        .expect_build_information_tag_to_be_present = false,
    }}));

// Other modes result in a tag describing the build information. A compliant
// Simple or Base profile decoder should handle the presence of the tag, but
// it's not required to understand what it means.
INSTANTIATE_TEST_SUITE_P(
    MixPresentationTagsArePresentWithBuildInformationTag,
    MixPresentationTagsPresenceTest,
    testing::ValuesIn<MixPresentationTagsPresenceTestCase>(
        {{
             .include_mix_presentation_tags = true,
             .append_build_information_tag = true,
             .expected_num_tags = 1,
             .expect_build_information_tag_to_be_present = true,
         },
         {
             .include_mix_presentation_tags = false,
             .append_build_information_tag = true,
             .expected_num_tags = 1,
             .expect_build_information_tag_to_be_present = true,
         }}));

TEST(Generate, CopiesOutputMixGain) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  FillMixGainParamDefinition(kCommonParameterId, kNonZeroMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_output_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_output_mix_gain =
      generated_obus.front().sub_mixes_[0].output_mix_gain;
  EXPECT_EQ(first_output_mix_gain.parameter_id_, kCommonParameterId);
  EXPECT_EQ(first_output_mix_gain.parameter_rate_, kCommonParameterRate);
  EXPECT_EQ(first_output_mix_gain.param_definition_mode_, kParamDefinitionMode);
  EXPECT_EQ(first_output_mix_gain.reserved_, kParamDefinitionReserved);
  EXPECT_EQ(first_output_mix_gain.default_mix_gain_, kNonZeroMixGain);
}

TEST(Generate, CopiesElementMixGain) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  FillMixGainParamDefinition(kCommonParameterId, kNonZeroMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_audio_elements(0)
                                  ->mutable_element_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_element_mix_gain =
      generated_obus.front().sub_mixes_[0].audio_elements[0].element_mix_gain;
  EXPECT_EQ(first_element_mix_gain.parameter_id_, kCommonParameterId);
  EXPECT_EQ(first_element_mix_gain.parameter_rate_, kCommonParameterRate);
  EXPECT_EQ(first_element_mix_gain.param_definition_mode_,
            kParamDefinitionMode);
  EXPECT_EQ(first_element_mix_gain.reserved_, kParamDefinitionReserved);
  EXPECT_EQ(first_element_mix_gain.default_mix_gain_, kNonZeroMixGain);
}

TEST(Generate, CopiesDeprecatedOutputMixConfig) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0).mutable_sub_mixes(0)->clear_output_mix_gain();
  FillMixGainParamDefinition(kCommonParameterId, kNonZeroMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_output_mix_config()
                                  ->mutable_output_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_output_mix_gain =
      generated_obus.front().sub_mixes_[0].output_mix_gain;
  EXPECT_EQ(first_output_mix_gain.parameter_id_, kCommonParameterId);
  EXPECT_EQ(first_output_mix_gain.parameter_rate_, kCommonParameterRate);
  EXPECT_EQ(first_output_mix_gain.param_definition_mode_, kParamDefinitionMode);
  EXPECT_EQ(first_output_mix_gain.reserved_, kParamDefinitionReserved);
  EXPECT_EQ(first_output_mix_gain.default_mix_gain_, kNonZeroMixGain);
}

TEST(Generate, CopiesDeprecatedElementMixConfig) {
  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  mix_presentation_metadata.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->clear_element_mix_gain();
  FillMixGainParamDefinition(kCommonParameterId, kNonZeroMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_audio_elements(0)
                                  ->mutable_element_mix_config()
                                  ->mutable_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_element_mix_gain =
      generated_obus.front().sub_mixes_[0].audio_elements[0].element_mix_gain;
  EXPECT_EQ(first_element_mix_gain.parameter_id_, kCommonParameterId);
  EXPECT_EQ(first_element_mix_gain.parameter_rate_, kCommonParameterRate);
  EXPECT_EQ(first_element_mix_gain.param_definition_mode_,
            kParamDefinitionMode);
  EXPECT_EQ(first_element_mix_gain.reserved_, kParamDefinitionReserved);
  EXPECT_EQ(first_element_mix_gain.default_mix_gain_, kNonZeroMixGain);
}

TEST(Generate, NonDeprecatedElementMixConfigTakesPrecedence) {
  constexpr uint32_t kDeprecatedParameterId = 2000;
  constexpr uint32_t kNonDeprecatedParameterId = 3000;
  constexpr int16_t kDeprecatedElementMixGain = 100;
  constexpr int16_t kNonDeprecatedElementMixGain = 200;

  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  // When both both the deprecated and non-deprecated element mix config are
  // provided, the non-deprecated config takes precedence.
  FillMixGainParamDefinition(kNonDeprecatedParameterId,
                             kNonDeprecatedElementMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_audio_elements(0)
                                  ->mutable_element_mix_gain());
  FillMixGainParamDefinition(kDeprecatedParameterId, kDeprecatedElementMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_audio_elements(0)
                                  ->mutable_element_mix_config()
                                  ->mutable_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_element_mix_gain =
      generated_obus.front().sub_mixes_[0].audio_elements[0].element_mix_gain;
  EXPECT_EQ(first_element_mix_gain.parameter_id_, kNonDeprecatedParameterId);
  EXPECT_EQ(first_element_mix_gain.default_mix_gain_,
            kNonDeprecatedElementMixGain);
}

TEST(Generate, NonDeprecatedOutputMixConfigTakesPrecedence) {
  constexpr uint32_t kDeprecatedParameterId = 2000;
  constexpr uint32_t kNonDeprecatedParameterId = 3000;
  constexpr int16_t kDeprecatedElementMixGain = 100;
  constexpr int16_t kNonDeprecatedElementMixGain = 200;

  MixPresentationObuMetadatas mix_presentation_metadata;
  FillMixPresentationMetadata(mix_presentation_metadata.Add());
  // When both both the deprecated and non-deprecated element mix config are
  // provided, the non-deprecated config takes precedence.
  FillMixGainParamDefinition(kNonDeprecatedParameterId,
                             kNonDeprecatedElementMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_output_mix_gain());
  FillMixGainParamDefinition(kDeprecatedParameterId, kDeprecatedElementMixGain,
                             *mix_presentation_metadata.at(0)
                                  .mutable_sub_mixes(0)
                                  ->mutable_output_mix_config()
                                  ->mutable_output_mix_gain());
  MixPresentationGenerator generator(mix_presentation_metadata);

  std::list<MixPresentationObu> generated_obus;
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus),
              IsOk());

  const auto& first_output_mix_gain =
      generated_obus.front().sub_mixes_[0].output_mix_gain;
  EXPECT_EQ(first_output_mix_gain.parameter_id_, kNonDeprecatedParameterId);
  EXPECT_EQ(first_output_mix_gain.default_mix_gain_,
            kNonDeprecatedElementMixGain);
}

class MixPresentationGeneratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    FillMixPresentationMetadata(mix_presentation_metadata_.Add());

    AddMixPresentationObuWithAudioElementIds(
        kMixPresentationId, {kAudioElementId}, kCommonParameterId,
        kCommonParameterRate, expected_obus_);
  }

 protected:
  MixPresentationObuMetadatas mix_presentation_metadata_;
  std::list<MixPresentationObu> generated_obus_;
  std::list<MixPresentationObu> expected_obus_;
};

TEST_F(MixPresentationGeneratorTest, EmptyUserMetadataGeneratesNoObus) {
  MixPresentationGenerator generator(/*mix_presentation_metadata=*/{});
  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());

  EXPECT_TRUE(generated_obus_.empty());
}

TEST_F(MixPresentationGeneratorTest, SSConventionWithOneStereoAudioElement) {
  MixPresentationGenerator generator(mix_presentation_metadata_);
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus_),
              IsOk());
  EXPECT_EQ(generated_obus_, expected_obus_);
}

TEST_F(MixPresentationGeneratorTest, SupportsUtf8) {
  const std::string kUtf8FourByteSequenceCode = "\xf0\x9d\x85\x9e\x00)";
  mix_presentation_metadata_.at(0).set_count_label(1);
  mix_presentation_metadata_.at(0).add_localized_presentation_annotations(
      kUtf8FourByteSequenceCode);

  MixPresentationGenerator generator(mix_presentation_metadata_);
  ASSERT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());

  const auto generated_annotations =
      generated_obus_.back().GetLocalizedPresentationAnnotations();
  ASSERT_FALSE(generated_annotations.empty());
  EXPECT_EQ(generated_annotations[0], kUtf8FourByteSequenceCode);
}

TEST_F(MixPresentationGeneratorTest, InvalidHeadphonesRenderingMode) {
  mix_presentation_metadata_.at(0)
      .mutable_sub_mixes(0)
      ->mutable_audio_elements(0)
      ->mutable_rendering_config()
      ->set_headphones_rendering_mode(
          iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_INVALID);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_FALSE(
      generator.Generate(kAppendBuildInformationTag, generated_obus_).ok());
}

TEST_F(MixPresentationGeneratorTest, IgnoresDeprecatedNumSubMixes) {
  // This test assumes the default metadata has one sub mix.
  constexpr uint32_t kExpectedNumSubMixes = 1;
  ASSERT_EQ(mix_presentation_metadata_.at(0).sub_mixes_size(),
            kExpectedNumSubMixes);
  // Include a strange value for the deprecated `num_sub_mixes` field.
  constexpr uint32_t kIncorrectIgnoredNumSubMixes = 2;
  mix_presentation_metadata_.at(0).set_num_sub_mixes(
      kIncorrectIgnoredNumSubMixes);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());

  // Regardless of the deprecated `num_layouts` field, the number of layouts is
  // inferred the `layouts` array.
  EXPECT_EQ(generated_obus_.back().GetNumSubMixes(), kExpectedNumSubMixes);
  EXPECT_EQ(generated_obus_.back().sub_mixes_.size(), kExpectedNumSubMixes);
}

TEST_F(MixPresentationGeneratorTest, IgnoresDeprecatedNumAudioElements) {
  // This test assumes the default metadata has one audio element.
  constexpr uint32_t kExpectedNumAudioElements = 1;
  ASSERT_EQ(mix_presentation_metadata_.at(0).sub_mixes(0).audio_elements_size(),
            kExpectedNumAudioElements);
  // Include a strange value for the deprecated `num_audio_elements`.
  constexpr uint32_t kIncorrectIgnoredNumAudioElements = 2;
  mix_presentation_metadata_.at(0).mutable_sub_mixes(0)->set_num_audio_elements(
      kIncorrectIgnoredNumAudioElements);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());

  // Regardless of the deprecated `num_audio_elements` field, the number of
  // audio elements the `audio_elements` array.
  EXPECT_EQ(generated_obus_.back().sub_mixes_[0].audio_elements.size(),
            kExpectedNumAudioElements);
}

TEST_F(MixPresentationGeneratorTest, IgnoresDeprecatedNumLayouts) {
  // This test assumes the default metadata has one layout.
  constexpr uint32_t kExpectedNumLayouts = 1;
  ASSERT_EQ(mix_presentation_metadata_.at(0).sub_mixes(0).layouts().size(),
            kExpectedNumLayouts);
  // Include a strange value for the deprecated `num_layouts`.
  constexpr uint32_t kIncorrectIgnoredNumLayouts = 2;
  mix_presentation_metadata_.at(0).mutable_sub_mixes(0)->set_num_layouts(
      kIncorrectIgnoredNumLayouts);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());

  // Regardless of the deprecated `num_layouts` field, the number of layouts is
  // inferred from the `layouts` array.
  EXPECT_EQ(generated_obus_.back().sub_mixes_[0].layouts.size(),
            kExpectedNumLayouts);
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

  EXPECT_THAT(generator.Generate(kAppendBuildInformationTag, generated_obus_),
              IsOk());
}

TEST_F(MixPresentationGeneratorTest, InvalidLayoutType) {
  mix_presentation_metadata_.at(0)
      .mutable_sub_mixes(0)
      ->mutable_layouts(0)
      ->mutable_loudness_layout()
      ->set_layout_type(iamf_tools_cli_proto::LAYOUT_TYPE_INVALID);
  MixPresentationGenerator generator(mix_presentation_metadata_);

  EXPECT_FALSE(
      generator.Generate(kAppendBuildInformationTag, generated_obus_).ok());
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
                               LoudspeakersReservedOrBinauralLayout{.reserved =
                                                                        0}},
       .loudness = {.info_type = 0}}};

  MixPresentationGenerator generator(mix_presentation_metadata_);
  EXPECT_THAT(generator.Generate(kOmitBuildInformationTag, generated_obus_),
              IsOk());
  EXPECT_EQ(generated_obus_, expected_obus_);
}

TEST(CopySoundSystem, ValidSoundSystem) {
  iamf_tools_cli_proto::SoundSystem input_sound_system =
      iamf_tools_cli_proto::SOUND_SYSTEM_A_0_2_0;

  LoudspeakersSsConventionLayout::SoundSystem output_sound_system;
  EXPECT_THAT(MixPresentationGenerator::CopySoundSystem(input_sound_system,
                                                        output_sound_system),
              IsOk());
  EXPECT_EQ(output_sound_system,
            LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0);
}

TEST(CopySoundSystem, InvalidSoundSystem) {
  iamf_tools_cli_proto::SoundSystem input_sound_system =
      iamf_tools_cli_proto::SOUND_SYSTEM_INVALID;

  LoudspeakersSsConventionLayout::SoundSystem output_sound_system;
  EXPECT_FALSE(MixPresentationGenerator::CopySoundSystem(input_sound_system,
                                                         output_sound_system)
                   .ok());
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
          anchor_elements:
          [ { anchor_element: ANCHOR_TYPE_DIALOGUE anchored_loudness: 1000 }
            , { anchor_element: ANCHOR_TYPE_ALBUM anchored_loudness: 1001 }]
      )pb",
      &user_loudness);

  // Configured expected data. The function only writes to the
  // `AnchoredLoudness`.
  const AnchoredLoudness expected_output_loudness = {
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

TEST(CopyUserAnchoredLoudness, IgnoresDeprecatedNumAnchoredLoudnessField) {
  // Set up an anchored loudness which no anchor elements, but incorrectly
  // claims there is one.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kAnchoredLoudness};
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  user_loudness.mutable_anchored_loudness()->set_num_anchored_loudness(1);

  EXPECT_THAT(MixPresentationGenerator::CopyUserAnchoredLoudness(
                  user_loudness, output_loudness),
              IsOk());

  // Regardless of the deprecated `num_anchored_loudness` field, the number of
  // anchor elements is inferred from the `anchor_elements` array.
  EXPECT_TRUE(output_loudness.anchored_loudness.anchor_elements.empty());
}

TEST(CopyUserAnchoredLoudness, IllegalUnknownAnchorElementEnum) {
  // `info_type` must be configured as a prerequisite.
  LoudnessInfo output_loudness = {.info_type = LoudnessInfo::kAnchoredLoudness};

  // Configure user data to copy in.
  iamf_tools_cli_proto::LoudnessInfo user_loudness;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        anchored_loudness {
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
