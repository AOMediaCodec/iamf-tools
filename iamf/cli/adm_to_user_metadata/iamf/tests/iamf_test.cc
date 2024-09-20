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

#include "iamf/cli/adm_to_user_metadata/iamf/iamf.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kFrameDurationMs = 10;
constexpr uint32_t kSamplesPerSec = 48000;

constexpr absl::string_view kStereoAudioPackFormatId = "AP_00010002";
constexpr absl::string_view kStereoAudioObjectId = "Stereo Audio Object";
constexpr absl::string_view kThirdOrderAmbisonicsAudioObjectId =
    "TOA Audio Object";

AudioObject GetStereoAudioObject(absl::string_view id = kStereoAudioObjectId) {
  return AudioObject(
      {.id = std::string(id),
       .audio_pack_format_id_refs = {std::string(kStereoAudioPackFormatId)}});
}

const AudioObject& GetThirdOrderAmbisonicsAudioObject() {
  constexpr absl::string_view kThirdOrderAmbisonicsAudioPackFormatId =
      "AP_00040003";

  static const absl::NoDestructor<AudioObject> kThirdOrderAmbisonicsAudioObject(
      {.id = std::string(kThirdOrderAmbisonicsAudioObjectId),
       .audio_pack_format_id_refs = {
           std::string(kThirdOrderAmbisonicsAudioPackFormatId)}});
  return *kThirdOrderAmbisonicsAudioObject;
}

const AudioObject& GetObjectBasedAudioObject() {
  constexpr absl::string_view kObjectBasedAudioObjectId =
      "Object Based Audio Object";
  constexpr absl::string_view kObjectBasedAudioPackFormatId = "AP_00030001";

  static const absl::NoDestructor<AudioObject> kStereoAudioObject(
      {.id = std::string(kObjectBasedAudioObjectId),
       .audio_pack_format_id_refs = {
           std::string(kObjectBasedAudioPackFormatId)}});
  return *kStereoAudioObject;
}

const ADM& GetAdmWithStereoAndToaObjectsWithoutAudioProgramme() {
  static const absl::NoDestructor<ADM> kAdmWithStereoAndToaObjects(
      {.audio_objects = {GetStereoAudioObject(),
                         GetThirdOrderAmbisonicsAudioObject()}});
  return *kAdmWithStereoAndToaObjects;
}

const ADM& GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes() {
  static const absl::NoDestructor<ADM> kAdmWithStereoAndToaObjects(
      {.audio_programmes =
           {{
                .id = "ProgrammeIdWithTwoAudioObjects",
                .audio_content_id_refs = {"AudioContentIdWithTwoObjects"},
            },
            {
                .id = "ProgrammeIdWithOneAudioObject",
                .audio_content_id_refs = {"AudioContentIdWithOneObject"},
            }},
       .audio_contents = {{
                              .id = "AudioContentIdWithTwoObjects",
                              .audio_object_id_ref =
                                  {std::string(kStereoAudioObjectId),
                                   std::string(
                                       kThirdOrderAmbisonicsAudioObjectId)},
                          },
                          {
                              .id = "AudioContentIdWithOneObject",
                              .audio_object_id_ref = {std::string(
                                  kThirdOrderAmbisonicsAudioObjectId)},
                          }},
       .audio_objects = {GetStereoAudioObject(),
                         GetThirdOrderAmbisonicsAudioObject()}});
  return *kAdmWithStereoAndToaObjects;
}

const absl::string_view kStereoWithComplementaryGroupId =
    "Stereo Object with Complementary Group";
const ADM& GetAdmWithComplementaryGroups() {
  static const absl::NoDestructor<ADM> kAdmWithComplementaryGroups(
      {.audio_programmes = {{
           .id = "AudioProgramWithStereoWithComplementaryGroup",
           .audio_content_id_refs = {"AudioContentIdWithComplemntaryGroup"},
       }},
       .audio_contents = {{
           .id = "AudioContentIdWithComplemntaryGroup",
           .audio_object_id_ref = {std::string(
               kStereoWithComplementaryGroupId)},
       }},
       .audio_objects = {{.id = std::string(kStereoWithComplementaryGroupId),
                          .audio_pack_format_id_refs =
                              {
                                  std::string(kStereoAudioPackFormatId),
                              },
                          .audio_comple_object_id_ref = {std::string(
                              kThirdOrderAmbisonicsAudioObjectId)}},
                         GetThirdOrderAmbisonicsAudioObject()}});
  return *kAdmWithComplementaryGroups;
}

TEST(Create, WithNoAudioObjectsSucceeds) {
  EXPECT_THAT(IAMF::Create(/*file_name=*/"",
                           /*adm=*/{}, kFrameDurationMs, kSamplesPerSec),
              IsOk());
}

TEST(Create, WithObjectBasedAudioObjectFails) {
  EXPECT_FALSE(
      IAMF::Create(/*file_name=*/"",
                   /*adm=*/{.audio_objects = {GetObjectBasedAudioObject()}},
                   kFrameDurationMs, kSamplesPerSec)
          .ok());
}

TEST(Create, PopulatesAudioFrameHandlerFilePrefix) {
  constexpr absl::string_view kExpectedFileNamePrefix = "test_file_prefix";

  const auto iamf = IAMF::Create(kExpectedFileNamePrefix, /*adm=*/{},
                                 kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_EQ(iamf->audio_frame_handler_.file_prefix_, kExpectedFileNamePrefix);
}

TEST(Create, PopulatesIamfInputLayoutsFromObjects) {
  const auto iamf =
      IAMF::Create("", GetAdmWithStereoAndToaObjectsWithoutAudioProgramme(),
                   kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  const std::vector<IamfInputLayout> kExpectedInputLayouts = {
      IamfInputLayout::kStereo, IamfInputLayout::kAmbisonicsOrder3};

  EXPECT_EQ(iamf->input_layouts_, kExpectedInputLayouts);

  EXPECT_TRUE(iamf->audio_object_to_audio_element_.empty());
  EXPECT_TRUE(iamf->mix_presentation_id_to_audio_objects_and_metadata_.empty());
}

TEST(Create, MapsAreEmptyWhenNoAudioProgramme) {
  const auto iamf =
      IAMF::Create("", GetAdmWithStereoAndToaObjectsWithoutAudioProgramme(),
                   kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_TRUE(iamf->audio_object_to_audio_element_.empty());
  EXPECT_TRUE(iamf->mix_presentation_id_to_audio_objects_and_metadata_.empty());
}

TEST(Create, PopulatesAudioObjectToAudioElement) {
  const auto iamf =
      IAMF::Create("", GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes(),
                   kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_EQ(iamf->audio_object_to_audio_element_.size(), 2);
  EXPECT_EQ(iamf->audio_object_to_audio_element_.at(
                std::string(kStereoAudioObjectId)),
            0);
  EXPECT_EQ(iamf->audio_object_to_audio_element_.at(
                std::string(kThirdOrderAmbisonicsAudioObjectId)),
            1);
}

TEST(Create, PopulatesAudioProgrammeToAudioObjectsMap) {
  constexpr int32_t kExpectedFirstAudioProgramIndex = 0;
  constexpr int32_t kExpectedFirstAudioProgramNumAudioObjects = 2;
  constexpr int32_t kExpectedSecondAudioProgramIndex = 1;
  constexpr int32_t kExpectedSecondAudioProgramNumAudioObjects = 1;

  const auto iamf =
      IAMF::Create("", GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes(),
                   kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_EQ(iamf->mix_presentation_id_to_audio_objects_and_metadata_.size(), 2);
  const auto& first_mix_presentation_data =
      iamf->mix_presentation_id_to_audio_objects_and_metadata_.at(0);
  EXPECT_EQ(first_mix_presentation_data.original_audio_programme_index,
            kExpectedFirstAudioProgramIndex);
  EXPECT_EQ(first_mix_presentation_data.audio_objects.size(),
            kExpectedFirstAudioProgramNumAudioObjects);
  EXPECT_EQ(first_mix_presentation_data.audio_objects.at(0).id,
            std::string(kStereoAudioObjectId));
  EXPECT_EQ(first_mix_presentation_data.audio_objects.at(1).id,
            std::string(kThirdOrderAmbisonicsAudioObjectId));

  const auto& second_mix_presentation_data =
      iamf->mix_presentation_id_to_audio_objects_and_metadata_.at(1);
  EXPECT_EQ(second_mix_presentation_data.original_audio_programme_index,
            kExpectedSecondAudioProgramIndex);
  EXPECT_EQ(second_mix_presentation_data.audio_objects.size(),
            kExpectedSecondAudioProgramNumAudioObjects);
  EXPECT_EQ(second_mix_presentation_data.audio_objects.at(0).id,
            std::string(kThirdOrderAmbisonicsAudioObjectId));
}

TEST(Create, IgnoresAudioProgrammeWithMoreThanTwoAudioObjects) {
  const absl::string_view kSecondStereoObjectId = "Another Stereo Audio Object";
  auto adm = GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes();
  adm.audio_programmes.push_back(
      {.id = "AudioProgrammeWithTooManyAudioObjects",
       .audio_content_id_refs = {"AudioContentIdWithThreeObjects"}});
  adm.audio_contents.push_back(
      {.id = "AudioContentIdWithThreeObjects",
       .audio_object_id_ref = {std::string(kStereoAudioObjectId),
                               std::string(kThirdOrderAmbisonicsAudioObjectId),
                               std::string(kSecondStereoObjectId)}});
  adm.audio_objects.push_back(GetStereoAudioObject(kSecondStereoObjectId));
  const auto iamf = IAMF::Create("", adm, kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_EQ(iamf->mix_presentation_id_to_audio_objects_and_metadata_.size(), 2);
}

TEST(Create, CreatesAudioProgrammesForComplementaryGroups) {
  const auto iamf = IAMF::Create("", GetAdmWithComplementaryGroups(),
                                 kFrameDurationMs, kSamplesPerSec);
  ASSERT_THAT(iamf, IsOk());

  EXPECT_EQ(iamf->audio_object_to_audio_element_.size(), 2);

  EXPECT_EQ(iamf->mix_presentation_id_to_audio_objects_and_metadata_.size(), 2);
  const auto& first_mix_presentation_data =
      iamf->mix_presentation_id_to_audio_objects_and_metadata_.at(0);
  EXPECT_EQ(first_mix_presentation_data.original_audio_programme_index, 0);
  EXPECT_EQ(first_mix_presentation_data.audio_objects.size(), 1);
  EXPECT_EQ(first_mix_presentation_data.audio_objects.at(0).id,
            kStereoWithComplementaryGroupId);

  const auto& second_mix_presentation_data =
      iamf->mix_presentation_id_to_audio_objects_and_metadata_.at(1);
  EXPECT_EQ(second_mix_presentation_data.original_audio_programme_index, 0);
  EXPECT_EQ(second_mix_presentation_data.audio_objects.size(), 1);
  EXPECT_EQ(second_mix_presentation_data.audio_objects.at(0).id,
            kThirdOrderAmbisonicsAudioObjectId);
}

struct FrameDurationInfoTestCase {
  int32_t frame_duration_ms;
  int64_t samples_per_sec;
  int32_t expected_num_samples_per_frame = 0;
  bool is_valid = true;
};

using FrameDurationInfo = ::testing::TestWithParam<FrameDurationInfoTestCase>;

TEST_P(FrameDurationInfo, TestWithParam) {
  const auto& test_case = GetParam();
  // Many fields are irrelevant to calculating the number of samples per frame.
  auto iamf = IAMF::Create(/*file_name=*/"",
                           /*adm=*/{}, test_case.frame_duration_ms,
                           test_case.samples_per_sec);
  ASSERT_EQ(iamf.ok(), test_case.is_valid);

  if (test_case.is_valid) {
    EXPECT_EQ(iamf->num_samples_per_frame_,
              test_case.expected_num_samples_per_frame);
  }
}

INSTANTIATE_TEST_SUITE_P(CalculatesNumSamplesPerFrame, FrameDurationInfo,
                         testing::ValuesIn<FrameDurationInfoTestCase>({
                             {.frame_duration_ms = 10,
                              .samples_per_sec = 48000,
                              .expected_num_samples_per_frame = 480},
                             {.frame_duration_ms = 20,
                              .samples_per_sec = 48000,
                              .expected_num_samples_per_frame = 960},
                             {.frame_duration_ms = 20,
                              .samples_per_sec = 96000,
                              .expected_num_samples_per_frame = 1920},
                             {.frame_duration_ms = 10,
                              .samples_per_sec = 44100,
                              .expected_num_samples_per_frame = 441},
                         }));

INSTANTIATE_TEST_SUITE_P(CalculatesNumSamplesPerFrameAndRoundsDown,
                         FrameDurationInfo,
                         testing::ValuesIn<FrameDurationInfoTestCase>({
                             {.frame_duration_ms = 9,
                              .samples_per_sec = 44100,
                              .expected_num_samples_per_frame = 396},

                         }));

INSTANTIATE_TEST_SUITE_P(
    InvalidWheFrameDurationIsZero, FrameDurationInfo,
    testing::ValuesIn<FrameDurationInfoTestCase>({
        {.frame_duration_ms = 0, .samples_per_sec = 48000, .is_valid = false},
    }));

INSTANTIATE_TEST_SUITE_P(
    InvalidWhenSamplesPerSecIsZero, FrameDurationInfo,
    testing::ValuesIn<FrameDurationInfoTestCase>({
        {.frame_duration_ms = 10, .samples_per_sec = 0, .is_valid = false},
    }));

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
