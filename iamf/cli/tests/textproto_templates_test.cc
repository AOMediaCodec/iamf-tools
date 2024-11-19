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
#include <filesystem>
#include <string>
#include <vector>

// [internal] Placeholder for get runfiles header.
#include "absl/log/log.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/encoder_main_lib.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
constexpr absl::string_view kIgnoredOutputPath = "";

struct TextprotoTemplateTestCase {
  absl::string_view textproto_filename;
  std::vector<absl::string_view> wav_filenames;
};

using TextprotoTemplate = ::testing::TestWithParam<TextprotoTemplateTestCase>;

// Validate that the textproto templates will encode successful.
TEST_P(TextprotoTemplate, ValidateTextprotos) {
  const TextprotoTemplateTestCase& test_case = GetParam();

  // Get the location of test wav files.
  static const auto input_wav_dir =
      std::filesystem::current_path() / std::string("iamf/cli/testdata");

  // Get the textproto to test.
  const auto user_metadata_filename =
      std::filesystem::current_path() /
      std::string("iamf/cli/textproto_templates") /
      test_case.textproto_filename;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseUserMetadataAssertSuccess(user_metadata_filename.string(),
                                 user_metadata);

  // Clear `file_name_prefix`; we only care about the status and not the output
  // files.
  user_metadata.mutable_test_vector_metadata()->clear_file_name_prefix();
  LOG(INFO) << "Testing with " << test_case.textproto_filename;

  // Replace the wav filenames.
  ASSERT_EQ(user_metadata.audio_frame_metadata_size(),
            test_case.wav_filenames.size());
  for (int i = 0; i < test_case.wav_filenames.size(); ++i) {
    user_metadata.mutable_audio_frame_metadata(i)->set_wav_filename(
        test_case.wav_filenames[i]);
  }

  // Call encoder and check that the encoding was successful.
  const absl::Status result =
      iamf_tools::TestMain(user_metadata, input_wav_dir.string().c_str(),
                           std::string(kIgnoredOutputPath));

  EXPECT_THAT(result, IsOk()) << "File= " << test_case.textproto_filename;
}

INSTANTIATE_TEST_SUITE_P(PcmStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"stereo_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

INSTANTIATE_TEST_SUITE_P(OpusStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"stereo_opus.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Pcm5dot1, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"5dot1_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Opus5dot1, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"5dot1_opus.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Pcm5dot1dot2, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"5dot1dot2_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Opus5dot1dot2, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"5dot1dot2_opus.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Pcm7dot1dot4, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"7dot1dot4_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(Opus7dot1dot4, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"7dot1dot4_opus.textproto",
                               std::vector<absl::string_view>{
                                   "Mechanism_5s.wav"}}}));

INSTANTIATE_TEST_SUITE_P(PcmFoa, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"1OA_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_foa_48khz.wav"}}}));

INSTANTIATE_TEST_SUITE_P(OpusFoa, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"1OA_opus.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_foa_48khz.wav"}}}));

INSTANTIATE_TEST_SUITE_P(PcmToa, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"3OA_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_8000_toa_48khz.wav"}}}));

INSTANTIATE_TEST_SUITE_P(OpusToa, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"3OA_opus.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_8000_toa_48khz.wav"}}}));

INSTANTIATE_TEST_SUITE_P(PcmFoaAndStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"1OA_and_stereo_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_foa_48khz.wav",
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

INSTANTIATE_TEST_SUITE_P(OpusFoaAndStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"1OA_and_stereo_opus.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_10000_foa_48khz.wav",
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

INSTANTIATE_TEST_SUITE_P(PcmToaAndStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"3OA_and_stereo_pcm24bit.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_8000_toa_48khz.wav",
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

INSTANTIATE_TEST_SUITE_P(OpusToaAndStereo, TextprotoTemplate,
                         testing::ValuesIn<TextprotoTemplateTestCase>(
                             {{"3OA_and_stereo_opus.textproto",
                               std::vector<absl::string_view>{
                                   "sawtooth_8000_toa_48khz.wav",
                                   "sawtooth_10000_stereo_48khz_s24le.wav"}}}));

}  // namespace
}  // namespace iamf_tools
