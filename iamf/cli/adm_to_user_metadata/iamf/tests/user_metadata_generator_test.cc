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

#include "iamf/cli/adm_to_user_metadata/iamf/user_metadata_generator.h"

#include <filesystem>
#include <string>
#include <string_view>

#include "absl/base/no_destructor.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

constexpr absl::string_view kStereoAudioObjectId = "Stereo Audio Object";
const AudioObject& GetStereoAudioObject() {
  constexpr absl::string_view kStereoAudioPackFormatId = "AP_00010001";

  static const absl::NoDestructor<AudioObject> kStereoAudioObject(
      {.id = std::string(kStereoAudioObjectId),
       .audio_pack_format_id_refs = {std::string(kStereoAudioPackFormatId)}});

  return *kStereoAudioObject;
}

constexpr absl::string_view kThirdOrderAmbisonicsAudioObjectId =
    "Third Order Ambisonics Audio Object";
const AudioObject& GetThirdOrderAmbisonicsAudioObject() {
  constexpr absl::string_view kThirdOrderAmbisonicsAudioPackFormatId =
      "AP_00040003";

  static const absl::NoDestructor<AudioObject> kThirdOrderAmbisonicsAudioObject(
      {.id = std::string(kThirdOrderAmbisonicsAudioObjectId),
       .audio_pack_format_id_refs = {
           std::string(kThirdOrderAmbisonicsAudioPackFormatId)}});
  return *kThirdOrderAmbisonicsAudioObject;
}

const ADM& GetAdmWithStereoObject() {
  static const absl::NoDestructor<ADM> kAdmWithStereoObject(
      {.audio_objects = {GetStereoAudioObject()}});

  return *kAdmWithStereoObject;
}

const ADM& GetAdmWithStereoAndToaObjectWithoutAudioProgrammes() {
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

constexpr FormatInfoChunk kFormatInfoChunk = {
    .num_channels = 1, .samples_per_sec = 48000, .bits_per_sample = 16};
constexpr int kMaxFrameDuration = 10;

using iamf_tools_cli_proto::UserMetadata;

TEST(Constructor, DoesNotCrash) {
  const UserMetadataGenerator user_metadata_generator(
      GetAdmWithStereoObject(), kFormatInfoChunk, kMaxFrameDuration);
}

TEST(WriteUserMetadataToFile, CreatesTextprotoFile) {
  UserMetadata user_metadata;
  const absl::string_view kFileNamePrefix = "prefix";
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix(
      kFileNamePrefix);
  const auto kOutputDirectory = GetAndCreateOutputDirectory("");
  const auto kExpectedTextprotoPath =
      std::filesystem::path(kOutputDirectory) /
      absl::StrCat(kFileNamePrefix, ".textproto");

  EXPECT_THAT(
      UserMetadataGenerator::WriteUserMetadataToFile(
          /*write_binary_proto=*/false, kOutputDirectory, user_metadata),
      IsOk());
  EXPECT_TRUE(std::filesystem::exists(kExpectedTextprotoPath));
}

TEST(WriteUserMetadataToFile, CreatesBinaryProtoFile) {
  UserMetadata user_metadata;
  const absl::string_view kFileNamePrefix = "prefix";
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix(
      kFileNamePrefix);
  const auto kOutputDirectory = GetAndCreateOutputDirectory("");
  const auto kExpectedBinaryProtoPath =
      std::filesystem::path(kOutputDirectory) /
      absl::StrCat(kFileNamePrefix, ".binpb");

  EXPECT_THAT(UserMetadataGenerator::WriteUserMetadataToFile(
                  /*write_binary_proto=*/true, kOutputDirectory, user_metadata),
              IsOk());
  EXPECT_TRUE(std::filesystem::exists(kExpectedBinaryProtoPath));
}

UserMetadata GenerateUserMetadataExpectOk(
    const ADM& adm, std::string_view input_file_prefix = "") {
  const UserMetadataGenerator user_metadata_generator(adm, kFormatInfoChunk,
                                                      kMaxFrameDuration);

  const auto user_metadata =
      user_metadata_generator.GenerateUserMetadata(input_file_prefix);
  EXPECT_THAT(user_metadata, IsOk()) << user_metadata.status();
  return *user_metadata;
}

TEST(GenerateUserMetadata, PopulatesTestVectorMetadaFilePrefix) {
  const absl::string_view kInputfilePrefix = "example_file_prefix";

  const auto& user_metadata =
      GenerateUserMetadataExpectOk(GetAdmWithStereoObject(), kInputfilePrefix);

  EXPECT_EQ(user_metadata.test_vector_metadata().file_name_prefix(),
            kInputfilePrefix);
}

TEST(GenerateUserMetadata, GeneratesOneOfEachDescriptorObuWithOneStereoObject) {
  const auto& user_metadata =
      GenerateUserMetadataExpectOk(GetAdmWithStereoObject());

  EXPECT_EQ(user_metadata.ia_sequence_header_metadata().size(), 1);
  EXPECT_EQ(user_metadata.codec_config_metadata().size(), 1);
  EXPECT_EQ(user_metadata.audio_element_metadata().size(), 1);
  EXPECT_EQ(user_metadata.mix_presentation_metadata().size(), 1);
}

TEST(GenerateUserMetadata,
     GeneratesOneAudioElementAndAudioFrameMetadataPerObject) {
  const auto& user_metadata =
      GenerateUserMetadataExpectOk(GetAdmWithStereoObject());

  EXPECT_EQ(user_metadata.audio_element_metadata().size(), 1);
  EXPECT_EQ(user_metadata.audio_frame_metadata().size(), 1);
}

TEST(GenerateUserMetadata,
     OnlyUsesFirstAudioObjectWhenThereAreNoAudioProgrammes) {
  // TODO(b/331401953): Consider changing the behavior to use all audio objects
  //                    when there are no audio_programmes.
  const auto& user_metadata = GenerateUserMetadataExpectOk(
      GetAdmWithStereoAndToaObjectWithoutAudioProgrammes());

  EXPECT_EQ(user_metadata.audio_element_metadata().size(), 1);

  EXPECT_EQ(user_metadata.mix_presentation_metadata().size(), 1);
  EXPECT_EQ(user_metadata.mix_presentation_metadata(0)
                .sub_mixes(0)
                .num_audio_elements(),
            1);
}

TEST(GenerateUserMetadata, UsesAllAudioObjectsInAudioProgrammes) {
  const auto& user_metadata = GenerateUserMetadataExpectOk(
      GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes());

  EXPECT_EQ(user_metadata.audio_element_metadata().size(), 2);
}

TEST(GenerateUserMetadata,
     GeneratesOneMixPresentationMetadataPerAudioProgramme) {
  constexpr auto kExpectedFirstMixPresentationNumAudioElements = 2;
  constexpr auto kExpectedSecondMixPresentationNumAudioElements = 1;
  const auto& user_metadata = GenerateUserMetadataExpectOk(
      GetAdmWithStereoAndToaObjectsAndTwoAudioProgrammes());

  EXPECT_EQ(user_metadata.mix_presentation_metadata().size(), 2);
  EXPECT_EQ(user_metadata.mix_presentation_metadata(0).num_sub_mixes(), 1);
  EXPECT_EQ(user_metadata.mix_presentation_metadata(0)
                .sub_mixes(0)
                .num_audio_elements(),
            kExpectedFirstMixPresentationNumAudioElements);

  EXPECT_EQ(user_metadata.mix_presentation_metadata(1).num_sub_mixes(), 1);
  EXPECT_EQ(user_metadata.mix_presentation_metadata(1)
                .sub_mixes(0)
                .num_audio_elements(),
            kExpectedSecondMixPresentationNumAudioElements);
}

TEST(GenerateUserMetadata, GeneratesNoParameterBlockOrArbitraryObus) {
  const auto& user_metadata =
      GenerateUserMetadataExpectOk(GetAdmWithStereoObject());

  EXPECT_TRUE(user_metadata.parameter_block_metadata().empty());
  EXPECT_TRUE(user_metadata.arbitrary_obu_metadata().empty());
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
