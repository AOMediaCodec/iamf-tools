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

#include "iamf/cli/adm_to_user_metadata/adm/xml_to_adm.h"

#include <string>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using testing::ElementsAreArray;

constexpr absl::string_view kAudioPackFormatIdMono = "AP_00010001";

constexpr int kImportanceThreshold = 0;

TEST(ParseXmlToAdm, InvalidXml) {
  EXPECT_FALSE(ParseXmlToAdm(R"xml(<open_tag> </mismatching_close_tag>)xml",
                             kImportanceThreshold)
                   .ok());
}

TEST(ParseXmlToAdm, LoadsAudioProgrammes) {
  const auto adm = ParseXmlToAdm(
      R"xml(
        <audioProgramme audioProgrammeID="audio_programme_id" audioProgrammeName="audio_programme_name" audioProgrammeLabel="audio_programme_label">
          <audioContentIDRef>audio_content_id</audioContentIDRef>
          <audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>
        </audioProgramme>
  )xml",
      kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  ASSERT_FALSE(adm->audio_programmes.empty());
  const auto& audio_programme = adm->audio_programmes[0];
  EXPECT_EQ(audio_programme.id, "audio_programme_id");
  EXPECT_EQ(audio_programme.name, "audio_programme_name");
  EXPECT_EQ(audio_programme.audio_programme_label, "audio_programme_label");
  EXPECT_THAT(audio_programme.audio_content_id_refs,
              ElementsAreArray({"audio_content_id"}));
  EXPECT_THAT(audio_programme.authoring_information.reference_layout
                  .audio_pack_format_id_ref,
              ElementsAreArray({kAudioPackFormatIdMono}));
}

TEST(ParseXmlToAdm, LoadsAudioContents) {
  const auto adm = ParseXmlToAdm(R"xml(
    <audioContent audioContentID="audio_content_id" audioContentName="audio_content_name">
      <audioObjectIDRef>object_1</audioObjectIDRef>
    </audioContent>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  ASSERT_FALSE(adm->audio_contents.empty());
  const auto& audio_content = adm->audio_contents[0];
  EXPECT_EQ(audio_content.id, "audio_content_id");
  EXPECT_EQ(audio_content.name, "audio_content_name");
  EXPECT_THAT(audio_content.audio_object_id_ref,
              ElementsAreArray({"object_1"}));
}

TEST(ParseXmlToAdm, LoadsAudioObject) {
  const auto adm = ParseXmlToAdm(R"xml(
  <audioObject audioObjectID="object_1" audioObjectName="object_name" importance="9">
    <audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>
    <audioTrackUIDRef>audio_track_uid_1</audioTrackUIDRef>
    <audioObjectLabel>audio_object_label</audioObjectLabel>
    <audioComplementaryObjectIDRef>complementary_object_id_ref</audioComplementaryObjectIDRef>
    <gain>2.5</gain>
  </audioObject>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  ASSERT_FALSE(adm->audio_objects.empty());
  const auto& audio_object = adm->audio_objects[0];
  EXPECT_EQ(audio_object.id, "object_1");
  EXPECT_EQ(audio_object.name, "object_name");
  EXPECT_EQ(audio_object.audio_object_label, "audio_object_label");
  EXPECT_EQ(audio_object.importance, 9);
  EXPECT_FLOAT_EQ(audio_object.gain, 2.5f);
  EXPECT_THAT(audio_object.audio_pack_format_id_refs,
              ElementsAreArray({kAudioPackFormatIdMono}));
  EXPECT_THAT(audio_object.audio_comple_object_id_ref,
              ElementsAreArray({"complementary_object_id_ref"}));
  EXPECT_THAT(audio_object.audio_track_uid_ref,
              ElementsAreArray({"audio_track_uid_1"}));
}

TEST(ParseXmlToAdm, LoudspeakerLayoutIsSupported) {
  const auto adm = ParseXmlToAdm(R"xml(
  <TopLevelElement>
    <audioObject audioObjectID="Mono">
        <audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="Stereo">
        <audioPackFormatIDRef>AP_00010002</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="5.1">
        <audioPackFormatIDRef>AP_00010003</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="5.1.2">
        <audioPackFormatIDRef>AP_00010004</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="5.1.4">
        <audioPackFormatIDRef>AP_00010005</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="7.1">
        <audioPackFormatIDRef>AP_0001000f</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="7.1.4">
        <audioPackFormatIDRef>AP_00010017</audioPackFormatIDRef>
    </audioObject>
  </TopLevelElement>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  EXPECT_EQ(adm->audio_objects.size(), 7);
}

TEST(ParseXmlToAdm, AmbisonicsLayoutIsSupported) {
  const auto adm = ParseXmlToAdm(R"xml(
  <TopLevelElement>
    <audioObject audioObjectID="FOA">
        <audioPackFormatIDRef>AP_00040001</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="SOA">
        <audioPackFormatIDRef>AP_00040002</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="TOA">
        <audioPackFormatIDRef>AP_00040003</audioPackFormatIDRef>
    </audioObject>
  </TopLevelElement>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  EXPECT_EQ(adm->audio_objects.size(), 3);
}

TEST(ParseXmlToAdm, BinauralLayoutIsSupported) {
  const auto adm = ParseXmlToAdm(R"xml(
  <audioObject>
      <audioPackFormatIDRef>AP_00050001</audioPackFormatIDRef>
  </audioObject>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  EXPECT_EQ(adm->audio_objects.size(), 1);
}

TEST(ParseXmlToAdm, FiltersOutUnsupportedLayouts) {
  const auto adm = ParseXmlToAdm(R"xml(
  <TopLevelElement>
    <audioObject audioObjectID="Mono">
        <audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="UnsupportedUserDefinedLoudspeakerLayout">
        <audioPackFormatIDRef>AP_00011000</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="UnsupportedLoudspeakerLayout">
        <audioPackFormatIDRef>AP_00010006</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="UnsupportedAmbisonicsLayout">
        <audioPackFormatIDRef>AP_00040004</audioPackFormatIDRef>
    </audioObject>
    <audioObject audioObjectID="UnsupportedBinauralLayout">
        <audioPackFormatIDRef>AP_00050000</audioPackFormatIDRef>
    </audioObject>
  </TopLevelElement>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  EXPECT_EQ(adm->audio_objects.size(), 1);
  EXPECT_EQ(adm->audio_objects[0].id, "Mono");
}

TEST(ParseXmlToAdm, AudioObjectImportanceDefaultsToTen) {
  const auto adm = ParseXmlToAdm(R"xml(
  <audioObject></audioObject>
  )xml",
                                 kImportanceThreshold);
  ASSERT_TRUE(adm.ok());

  EXPECT_EQ(adm->audio_objects[0].importance, 10);
}

TEST(ParseXmlToAdm, FiltersOutLowImportanceAudioObjects) {
  const std::string xml = R"xml(
  <topLevelElement>
    <audioObject importance="9"/>
    <audioObject importance="7"/>
    <audioObject importance="4"/>
    <audioObject importance="1"/>
  </topLevelElement>
  )xml";

  const auto adm_with_all_objects_below_threshold =
      ParseXmlToAdm(xml, /*importance_threshold=*/10);
  ASSERT_TRUE(adm_with_all_objects_below_threshold.ok());
  EXPECT_EQ(adm_with_all_objects_below_threshold->audio_objects.size(), 0);

  // One object is at or above the threshold.
  const auto adm_with_one_object_at_or_above_threshold =
      ParseXmlToAdm(xml, /*importance_threshold=*/9);
  ASSERT_TRUE(adm_with_one_object_at_or_above_threshold.ok());
  EXPECT_EQ(adm_with_one_object_at_or_above_threshold->audio_objects.size(), 1);

  // Three objects are at or above the threshold.
  const auto adm_with_three_objects_at_or_above_threshold =
      ParseXmlToAdm(xml, /*importance_threshold=*/3);
  ASSERT_TRUE(adm_with_three_objects_at_or_above_threshold.ok());
  EXPECT_EQ(adm_with_three_objects_at_or_above_threshold->audio_objects.size(),
            3);
}

TEST(ParseXmlToAdm, InvalidWhenImportanceIsNonInteger) {
  const std::string xml = R"xml(
    <audioObject importance="1.1"/>
  )xml";

  EXPECT_FALSE(ParseXmlToAdm(xml, /*importance_threshold=*/10).ok());
}

TEST(ParseXmlToAdm, InvalidWhenGainIsNonFloat) {
  EXPECT_FALSE(ParseXmlToAdm(R"xml(
    <audioObject>
      <gain>1-1</gain>
    </audioObject>)xml",
                             /*importance_threshold=*/10)
                   .ok());
}

TEST(ParseXmlToAdm, SetsExplicitLoudnessValuesAsFloat) {
  const auto adm = ParseXmlToAdm(
      R"xml(
        <audioProgramme>
          <integratedLoudness>1.1</integratedLoudness>
          <maxTruePeak>2.2</maxTruePeak>
          <dialogueLoudness>3.3</dialogueLoudness>
        </audioProgramme>
  )xml",
      kImportanceThreshold);
  ASSERT_TRUE(adm.ok());
  ASSERT_FALSE(adm->audio_programmes.empty());

  const auto& loudness_metadata = adm->audio_programmes[0].loudness_metadata;
  EXPECT_FLOAT_EQ(loudness_metadata.integrated_loudness, 1.1f);
  EXPECT_FLOAT_EQ(*loudness_metadata.max_true_peak, 2.2f);
  EXPECT_FLOAT_EQ(*loudness_metadata.dialogue_loudness, 3.3f);
}

TEST(ParseXmlToAdm, InvalidWhenFloatCannotBeParsed) {
  EXPECT_FALSE(ParseXmlToAdm(
                   R"xml(
        <audioProgramme>
          <integratedLoudness>1.1q</integratedLoudness>
        </audioProgramme>)xml",
                   kImportanceThreshold)
                   .ok());
}

TEST(ParseXmlToAdm, DefaultLoudnessValues) {
  const float kDefaultIntegratedLoudness = 0.0;
  const auto adm = ParseXmlToAdm(
      R"xml(
        <audioProgramme>
        </audioProgramme>
  )xml",
      kImportanceThreshold);
  ASSERT_TRUE(adm.ok());
  ASSERT_FALSE(adm->audio_programmes.empty());

  // The IAMF bitstream always needs `integrated_loudness`. The parser will
  // set it to 0 if it is not present in the XML.
  const auto& loudness_metadata = adm->audio_programmes[0].loudness_metadata;
  EXPECT_FLOAT_EQ(loudness_metadata.integrated_loudness,
                  kDefaultIntegratedLoudness);
  // The IAMF bitstream optionally uses additional loudness values. The parser
  // will set them to `std::nullopt` if they are not present in the XML.
  EXPECT_FALSE(loudness_metadata.dialogue_loudness.has_value());
  EXPECT_FALSE(loudness_metadata.max_true_peak.has_value());
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
