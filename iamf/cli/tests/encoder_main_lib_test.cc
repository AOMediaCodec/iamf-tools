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
#include "iamf/cli/encoder_main_lib.h"

#include <cstdint>
#include <filesystem>
#include <string>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/encoder_control_metadata.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/output_audio_format.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr absl::string_view kTestdataPath = "iamf/cli/testdata/";
constexpr absl::string_view kIgnoredOutputPath = "";
constexpr absl::string_view kTest000005ExpectedWavFilename =
    "test_000005_rendered_id_42_sub_mix_0_layout_0.wav";
const int kTest000005ExpectedWavBitDepth = 16;

using enum iamf_tools_cli_proto::OutputAudioFormat;

void AddIaSequenceHeader(iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        primary_profile: PROFILE_VERSION_SIMPLE
        additional_profile: PROFILE_VERSION_SIMPLE
      )pb",
      user_metadata.add_ia_sequence_header_metadata()));
}

void AddCodecConfig(iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 64
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_size: 16
            sample_rate: 48000
          }
        }
      )pb",
      user_metadata.add_codec_config_metadata()));
}

void ParseTestVectorAssertSuccess(
    absl::string_view textproto_filename, std::string& wav_directory,
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  wav_directory = GetRunfilesPath(kTestdataPath);
  // Get and parse the textproto to test.
  const std::string user_metadata_filename =
      GetRunfilesFile(kTestdataPath, textproto_filename);
  ParseUserMetadataAssertSuccess(user_metadata_filename, user_metadata);
}

TEST(EncoderMainLibTest, EmptyUserMetadataTestMainFails) {
  EXPECT_FALSE(TestMain(iamf_tools_cli_proto::UserMetadata(), "", "").ok());
}

TEST(EncoderMainLibTest, IaSequenceHeaderOnly) {
  // Populate the user metadata with only an IA Sequence Header, leaving
  // everything else empty. This will fail if
  // `partition_mix_gain_parameter_blocks` is left true (the default value).
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  EXPECT_FALSE(
      TestMain(user_metadata, "", std::string(kIgnoredOutputPath)).ok());

  // After setting `partition_mix_gain_parameter_blocks` to false, `TestMain()`
  // will succeed.
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);
  EXPECT_THAT(TestMain(user_metadata, "", std::string(kIgnoredOutputPath)),
              IsOk());
}

TEST(EncoderMainLibTest, IaSequenceHeaderAndCodecConfigSucceeds) {
  // Populate the user metadata with an IA Sequence Header AND a Codec Config,
  // leaving everything else empty. This will succeed.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  EXPECT_THAT(TestMain(user_metadata, "", std::string(kIgnoredOutputPath)),
              IsOk());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideSucceeds) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Configure a reasonable bit-depth to output to.
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(16);

  EXPECT_THAT(TestMain(user_metadata, "", std::string(kIgnoredOutputPath)),
              IsOk());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideHighSucceeds) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  const uint32_t kBitDepthTooHigh = 256;
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(kBitDepthTooHigh);

  // If wav writing was enabled then the configuration would be clamped to a
  // 32-bit file.
  EXPECT_THAT(TestMain(user_metadata, "", std::string(kIgnoredOutputPath)),
              IsOk());
}

TEST(EncoderMainLibTest,
     OutputRenderedFileFormatTakesPrecedenceOverDeprecatedOverrideBitDepth) {
  std::string wav_directory;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseTestVectorAssertSuccess("test_000005.textproto", wav_directory,
                               user_metadata);
  const auto output_iamf_directory = GetAndCreateOutputDirectory("");
  // Update controls to override the bit-depth with the deprecated
  // `output_wav_file_bit_depth_override`.
  constexpr uint32_t kDeprecatedOverrideBitDepth = 32;
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(kDeprecatedOverrideBitDepth);
  // `output_rendered_file_format` should take precedence over the deprecated
  // field.
  const auto kExpectedBitDepth = 24;
  user_metadata.mutable_encoder_control_metadata()
      ->set_output_rendered_file_format(
          OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR);

  EXPECT_THAT(TestMain(user_metadata, wav_directory, output_iamf_directory),
              IsOk());

  const auto expected_wav_path = std::filesystem::path(output_iamf_directory) /
                                 kTest000005ExpectedWavFilename;
  EXPECT_TRUE(std::filesystem::exists(expected_wav_path));
  const auto wav_reader = CreateWavReaderExpectOk(expected_wav_path.string());
  EXPECT_EQ(wav_reader.bit_depth(), kExpectedBitDepth);
}

TEST(EncoderMainLibTest, OutputRenderedFileFormatCanUseAutomaticBitDepth) {
  std::string wav_directory;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseTestVectorAssertSuccess("test_000005.textproto", wav_directory,
                               user_metadata);
  const auto output_iamf_directory = GetAndCreateOutputDirectory("");
  // Update controls to write out a wav file with automatic bit-depth.
  user_metadata.mutable_test_vector_metadata()
      ->clear_output_wav_file_bit_depth_override();
  user_metadata.mutable_encoder_control_metadata()
      ->set_output_rendered_file_format(OUTPUT_FORMAT_WAV_BIT_DEPTH_AUTOMATIC);

  EXPECT_THAT(TestMain(user_metadata, wav_directory, output_iamf_directory),
              IsOk());

  // The wav file matches the regular bit-depth of `test_000005.textproto`.
  const auto expected_wav_path = std::filesystem::path(output_iamf_directory) /
                                 kTest000005ExpectedWavFilename;
  EXPECT_TRUE(std::filesystem::exists(expected_wav_path));
  const auto wav_reader = CreateWavReaderExpectOk(expected_wav_path.string());
  EXPECT_EQ(wav_reader.bit_depth(), kTest000005ExpectedWavBitDepth);
}

TEST(EncoderMainLibTest, OutputRenderedFileFormatCanOverrideBitDepth) {
  std::string wav_directory;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseTestVectorAssertSuccess("test_000005.textproto", wav_directory,
                               user_metadata);
  const auto output_iamf_directory = GetAndCreateOutputDirectory("");
  // Update controls to write out a wav file with a specific bit-depth.
  user_metadata.mutable_test_vector_metadata()
      ->clear_output_wav_file_bit_depth_override();
  constexpr int kExpectedOverriddenBitDepth = 24;
  user_metadata.mutable_encoder_control_metadata()
      ->set_output_rendered_file_format(
          OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR);

  EXPECT_THAT(TestMain(user_metadata, wav_directory, output_iamf_directory),
              IsOk());

  // The wav file matches the overridden bit-depth.
  const auto expected_wav_path = std::filesystem::path(output_iamf_directory) /
                                 kTest000005ExpectedWavFilename;
  EXPECT_TRUE(std::filesystem::exists(expected_wav_path));
  const auto wav_reader = CreateWavReaderExpectOk(expected_wav_path.string());
  EXPECT_EQ(wav_reader.bit_depth(), kExpectedOverriddenBitDepth);
}

TEST(EncoderMainLibTest, OutputRenderedFileFormatCanDisableWavFileOutput) {
  std::string wav_directory;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseTestVectorAssertSuccess("test_000005.textproto", wav_directory,
                               user_metadata);
  const auto output_iamf_directory = GetAndCreateOutputDirectory("");
  // Update controls to disable writing a wav file.
  user_metadata.mutable_test_vector_metadata()
      ->clear_output_wav_file_bit_depth_override();
  user_metadata.mutable_encoder_control_metadata()
      ->set_output_rendered_file_format(
          iamf_tools_cli_proto::OUTPUT_FORMAT_NONE);

  EXPECT_THAT(TestMain(user_metadata, wav_directory, output_iamf_directory),
              IsOk());

  // The wav file is absent.
  const auto wav_path = std::filesystem::path(output_iamf_directory) /
                        kTest000005ExpectedWavFilename;
  EXPECT_FALSE(std::filesystem::exists(wav_path));
}

TEST(EncoderMainLibTest, SettingPrefixOutputsFile) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Setting a filename prefix makes the function output a .iamf file.
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix("empty");

  const auto output_iamf_directory = GetAndCreateOutputDirectory("");

  EXPECT_THAT(TestMain(user_metadata, "", output_iamf_directory), IsOk());

  EXPECT_TRUE(std::filesystem::exists(
      std::filesystem::path(output_iamf_directory) / "empty.iamf"));
}

TEST(EncoderMainLibTest, CreatesAndWritesToOutputIamfDirectory) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Setting a filename prefix makes the function output a .iamf file.
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix("empty");

  // Create a clean output directory.
  const auto test_directory_root = GetAndCreateOutputDirectory("");

  // The encoder will create and write the file based on a (nested)
  // `output_iamf_directory` argument.
  const auto output_iamf_directory =
      test_directory_root / std::filesystem::path("EncoderMainLibTest") /
      std::filesystem::path("CreatesAndWritesToOutputIamfDirectory");

  EXPECT_THAT(TestMain(user_metadata, "", output_iamf_directory.string()),
              IsOk());

  EXPECT_TRUE(std::filesystem::exists(output_iamf_directory / "empty.iamf"));
}

using TestVector = ::testing::TestWithParam<absl::string_view>;

// Validate the "is_valid" field in a test vector textproto file is consistent
// with the return value of `iamf_tools::TestMain()`.
TEST_P(TestVector, ValidateTestSuite) {
  // Get the location of test wav files.
  const auto textproto_filename = GetParam();
  std::string wav_directory;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ParseTestVectorAssertSuccess(textproto_filename, wav_directory,
                               user_metadata);

  // Call encoder. Clear `file_name_prefix`; we only care about the status and
  // not the output files.
  user_metadata.mutable_test_vector_metadata()->clear_file_name_prefix();
  // Skip checking the loudness is consistent with the user-provided data.
  // Loudness depends on coding and rendering details, and may slightly drift as
  // these change over time.
  user_metadata.mutable_test_vector_metadata()->set_validate_user_loudness(
      false);
  ABSL_LOG(INFO) << "Testing with " << textproto_filename;
  const absl::Status result = iamf_tools::TestMain(
      user_metadata, wav_directory, std::string(kIgnoredOutputPath));

  // Check if the result matches the expected value in the protos.
  if (user_metadata.test_vector_metadata().is_valid()) {
    EXPECT_THAT(result, IsOk()) << "File= " << textproto_filename;
  } else {
    EXPECT_FALSE(result.ok()) << " File= " << textproto_filename;
  }
}

// ---- Test Set 0 -----
INSTANTIATE_TEST_SUITE_P(InvalidTooLowTrim, TestVector,
                         testing::Values("test_000000_3.textproto"));

INSTANTIATE_TEST_SUITE_P(NopParamBlock, TestVector,
                         testing::Values("test_000002.textproto"));

INSTANTIATE_TEST_SUITE_P(NoTrimRequired, TestVector,
                         testing::Values("test_000005.textproto"));

INSTANTIATE_TEST_SUITE_P(UserRequestedTemporalDelimiters, TestVector,
                         testing::Values("test_000006.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidIASequenceHeaderIACode, TestVector,
                         testing::Values("test_000007.textproto"));

INSTANTIATE_TEST_SUITE_P(UserRequestedTrimAtEnd, TestVector,
                         testing::Values("test_000012.textproto"));

INSTANTIATE_TEST_SUITE_P(UserRequestedTrimAtStart, TestVector,
                         testing::Values("test_000013.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidPreskip, TestVector,
                         testing::Values("test_000014.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidDanglingFromDescriptorParameterBlock,
                         TestVector, testing::Values("test_000015.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidParameterBlockNotFullCoveringEnd, TestVector,
                         testing::Values("test_000016.textproto"));

INSTANTIATE_TEST_SUITE_P(FullFrameTrimmedAtEnd, TestVector,
                         testing::Values("test_000017.textproto"));

INSTANTIATE_TEST_SUITE_P(ExplicitAudioSubstreamID, TestVector,
                         testing::Values("test_000018.textproto"));

INSTANTIATE_TEST_SUITE_P(ParameterBlockStream, TestVector,
                         testing::Values("test_000019.textproto"));

// Batch 3:
INSTANTIATE_TEST_SUITE_P(Opus20ms, TestVector,
                         testing::Values("test_000020.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus40ms, TestVector,
                         testing::Values("test_000021.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidRollDistance, TestVector,
                         testing::Values("test_000022.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus5ms, TestVector,
                         testing::Values("test_000023.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus60ms, TestVector,
                         testing::Values("test_000024.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidVersion, TestVector,
                         testing::Values("test_000025.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidOutputChannelCount, TestVector,
                         testing::Values("test_000026.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidOutputGain, TestVector,
                         testing::Values("test_000027.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusInvalidMappingFamily, TestVector,
                         testing::Values("test_000028.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMLittleEndian16Bit48kHz, TestVector,
                         testing::Values("test_000029.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMLittleEndian16Bit44100Hz, TestVector,
                         testing::Values("test_000030.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMLittleEndian24Bit48kHz, TestVector,
                         testing::Values("test_000031.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus24kbps, TestVector,
                         testing::Values("test_000032.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus96kbps, TestVector,
                         testing::Values("test_000033.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusVoip, TestVector,
                         testing::Values("test_000034.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusLowdelay, TestVector,
                         testing::Values("test_000035.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMLayout5_1, TestVector,
                         testing::Values("test_000036.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus20Seconds, TestVector,
                         testing::Values("test_000037.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoLPCMInvalidOutputChannelCount, TestVector,
                         testing::Values("test_000040.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaAsToaProjectionLPCM, TestVector,
                         testing::Values("test_000044.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaProjectionOpusCoupledStereo, TestVector,
                         testing::Values("test_000048.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusLayout5_1, TestVector,
                         testing::Values("test_000049.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusFourLayerLayout7_1_4, TestVector,
                         testing::Values("test_000050.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusThreeLayerLayout7_1_2, TestVector,
                         testing::Values("test_000051.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTwoLayerLayout3_1_2, TestVector,
                         testing::Values("test_000052.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTwoLayerLayout7_1, TestVector,
                         testing::Values("test_000053.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusFourLayerLayout5_1_4, TestVector,
                         testing::Values("test_000054.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusThreeLayerLayout5_1_2, TestVector,
                         testing::Values("test_000055.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTwoLayerLayout5_1, TestVector,
                         testing::Values("test_000056.textproto"));

INSTANTIATE_TEST_SUITE_P(MixTwoStereoAudioElements, TestVector,
                         testing::Values("test_000058.textproto"));

INSTANTIATE_TEST_SUITE_P(ExplicitReconGain, TestVector,
                         testing::Values("test_000059.textproto"));

INSTANTIATE_TEST_SUITE_P(TwoLanguageLabels, TestVector,
                         testing::Values("test_000060.textproto"));

INSTANTIATE_TEST_SUITE_P(ExplicitDemixing, TestVector,
                         testing::Values("test_000061.textproto"));

INSTANTIATE_TEST_SUITE_P(TwoAnchorElements, TestVector,
                         testing::Values("test_000062.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidDuplicateAnchorElements, TestVector,
                         testing::Values("test_000063.textproto"));

INSTANTIATE_TEST_SUITE_P(ThreeDbDefaultMixGain, TestVector,
                         testing::Values("test_000064.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMFOALinearMixGain, TestVector,
                         testing::Values("test_000065.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMFOABezierLinearMixGain, TestVector,
                         testing::Values("test_000066.textproto"));

INSTANTIATE_TEST_SUITE_P(RenderingConfigExtension, TestVector,
                         testing::Values("test_000067.textproto"));

INSTANTIATE_TEST_SUITE_P(ConstantSubblockDurationEdgeCase, TestVector,
                         testing::Values("test_000068.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCM5_1_2To3_1_2, TestVector,
                         testing::Values("test_000069.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCM7_1_4To7_1_2, TestVector,
                         testing::Values("test_000070.textproto"));

INSTANTIATE_TEST_SUITE_P(MixGainDifferentParamDefinitionModes, TestVector,
                         testing::Values("test_000071.textproto"));

INSTANTIATE_TEST_SUITE_P(BasicStereoFLAC, TestVector,
                         testing::Values("test_000072.textproto"));

// TODO(b/360376661): Re-enable this test once the msan issue is fixed.
// INSTANTIATE_TEST_SUITE_P(
//     FLACLayout5_1, TestVector,
//     testing::Values("test_000073.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoFLAC, TestVector,
                         testing::Values("test_000074.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaMonoFLAC, TestVector,
                         testing::Values("test_000075.textproto"));

INSTANTIATE_TEST_SUITE_P(FrameAlignedAAC, TestVector,
                         testing::Values("test_000076.textproto"));

INSTANTIATE_TEST_SUITE_P(RedundantIASequenceHeaderAfter, TestVector,
                         testing::Values("test_000078.textproto"));

INSTANTIATE_TEST_SUITE_P(RedundantIASequenceHeaderBefore, TestVector,
                         testing::Values("test_000079.textproto"));

INSTANTIATE_TEST_SUITE_P(AppliedDefaultWNonzero, TestVector,
                         testing::Values("test_000080.textproto",
                                         "test_000081.textproto"));

INSTANTIATE_TEST_SUITE_P(IgnoredDefaultWNonzero, TestVector,
                         testing::Values("test_000082.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoLPCMHeadphonesRenderingMode1, TestVector,
                         testing::Values("test_000083.textproto"));

INSTANTIATE_TEST_SUITE_P(FLACInvalidRollDistance, TestVector,
                         testing::Values("test_000084.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMInvalidRollDistance, TestVector,
                         testing::Values("test_000085.textproto"));

INSTANTIATE_TEST_SUITE_P(FOAAndTwoLayer_5_1_2, TestVector,
                         testing::Values("test_000086.textproto"));

INSTANTIATE_TEST_SUITE_P(StereoAndTwoLayer_5_1, TestVector,
                         testing::Values("test_000087.textproto"));

INSTANTIATE_TEST_SUITE_P(ParamDefinitionMode0ExplicitSubblockDurations,
                         TestVector, testing::Values("test_000088.textproto"));

INSTANTIATE_TEST_SUITE_P(Scalable7_1_4HeadphonesRenderingMode1, TestVector,
                         testing::Values("test_000089.textproto"));

INSTANTIATE_TEST_SUITE_P(NonFrameAlignedAAC, TestVector,
                         testing::Values("test_000090.textproto"));

INSTANTIATE_TEST_SUITE_P(AACInvalidRollDistance, TestVector,
                         testing::Values("test_000091.textproto"));

INSTANTIATE_TEST_SUITE_P(AACLayout5_1, TestVector,
                         testing::Values("test_000092.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoAAC, TestVector,
                         testing::Values("test_000093.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaMonoAAC, TestVector,
                         testing::Values("test_000094.textproto"));

INSTANTIATE_TEST_SUITE_P(Scalable7_1_4LPCMBinauralLayout, TestVector,
                         testing::Values("test_000095.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaMonoLPCMBinauralLayout, TestVector,
                         testing::Values("test_000096.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMLittleEndian32Bit16kHz, TestVector,
                         testing::Values("test_000097.textproto"));

INSTANTIATE_TEST_SUITE_P(Opus32BitInput, TestVector,
                         testing::Values("test_000098.textproto"));

// ---- Test Set 1 -----

INSTANTIATE_TEST_SUITE_P(ZoaMonoLPCM, TestVector,
                         testing::Values("test_000100.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoLPCMHeadphonesRenderingMode0, TestVector,
                         testing::Values("test_000038.textproto",
                                         "test_000101.textproto"));

INSTANTIATE_TEST_SUITE_P(SoaMonoLPCM, TestVector,
                         testing::Values("test_000102.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaMonoLPCM, TestVector,
                         testing::Values("test_000039.textproto",
                                         "test_000103.textproto"));

INSTANTIATE_TEST_SUITE_P(ZoaProjectionLPCM, TestVector,
                         testing::Values("test_000104.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaProjectionLPCM, TestVector,
                         testing::Values("test_000042.textproto",
                                         "test_000105.textproto"));

INSTANTIATE_TEST_SUITE_P(SoaProjectionLPCM, TestVector,
                         testing::Values("test_000106.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaProjectionLPCM, TestVector,
                         testing::Values("test_000043.textproto",
                                         "test_000107.textproto"));
INSTANTIATE_TEST_SUITE_P(ZoaMonoOpus, TestVector,
                         testing::Values("test_000108.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaMonoOpus, TestVector,
                         testing::Values("test_000045.textproto",
                                         "test_000109.textproto"));

INSTANTIATE_TEST_SUITE_P(SoaMonoOpus, TestVector,
                         testing::Values("test_000110.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaMonoOpus, TestVector,
                         testing::Values("test_000046.textproto",
                                         "test_000111.textproto"));

INSTANTIATE_TEST_SUITE_P(ZoaProjectionOpus, TestVector,
                         testing::Values("test_000112.textproto"));

INSTANTIATE_TEST_SUITE_P(FoaProjectionOpus, TestVector,
                         testing::Values("test_000113.textproto"));

INSTANTIATE_TEST_SUITE_P(SoaProjectionOpus, TestVector,
                         testing::Values("test_000114.textproto"));

INSTANTIATE_TEST_SUITE_P(ToaProjectionOpus, TestVector,
                         testing::Values("test_000115.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedDescriptorAndTemporalUnitObus, TestVector,
                         testing::Values("test_000116.textproto"));

INSTANTIATE_TEST_SUITE_P(ObuExtensionFlag, TestVector,
                         testing::Values("test_000117.textproto"));

INSTANTIATE_TEST_SUITE_P(
    SimpleMixWithOneAudioElementAndBaseMixWithTwoAudioElements, TestVector,
    testing::Values("test_000118.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidCodecIdForSimpleProfile, TestVector,
                         testing::Values("test_000119.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidAudioElementTypeForSimpleProfile, TestVector,
                         testing::Values("test_000120.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedParameterTypeForSimpleProfile, TestVector,
                         testing::Values("test_000121.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedLoudspeakerLayoutAsFirstLayerForSimpleProfile,
                         TestVector, testing::Values("test_000122.textproto"));

INSTANTIATE_TEST_SUITE_P(
    BaseMixWithTwelveChannelsAndBaseEnhancedMixWithTwentyEightChannels,
    TestVector, testing::Values("test_000123.textproto"));

INSTANTIATE_TEST_SUITE_P(TwoSubmixes, TestVector,
                         testing::Values("test_000124.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedHeadphonesRenderingModeForSimpleProfile,
                         TestVector, testing::Values("test_000125.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedLayoutTypeForSimpleProfile, TestVector,
                         testing::Values("test_000126.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidTwoAudioElementsForSimpleProfile, TestVector,
                         testing::Values("test_000127.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidThreeAudioElementsForBaseProfile, TestVector,
                         testing::Values("test_000128.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedLoudspeakerLayoutAsSecondLayerForSimpleProfile,
                         TestVector, testing::Values("test_000129.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedAmbisonicsModeForSimpleProfile, TestVector,
                         testing::Values("test_000130.textproto"));

INSTANTIATE_TEST_SUITE_P(
    ReservedLoudnessLayoutForSimpleProfileWhichIsDefinedInBaseEnahncedProfile,
    TestVector, testing::Values("test_000131.textproto"));

INSTANTIATE_TEST_SUITE_P(
    SimpleMixWithTwoChannelsAndBaseEnhancedMixWithTwentySevenChannels,
    TestVector, testing::Values("test_000132.textproto"));

INSTANTIATE_TEST_SUITE_P(ParameterBlocksLongerDurationThanAudioFrames,
                         TestVector, testing::Values("test_000133.textproto"));

INSTANTIATE_TEST_SUITE_P(ExtensionsInIaSequenceHeader, TestVector,
                         testing::Values("test_000134.textproto"));

INSTANTIATE_TEST_SUITE_P(MultipleFramesTrimmedAtEnd, TestVector,
                         testing::Values("test_000135.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidInconsistentParamDefinitions, TestVector,
                         testing::Values("test_000136.textproto"));

// ---- Test Set 2 -----

INSTANTIATE_TEST_SUITE_P(BasicMonoLPCM, TestVector,
                         testing::Values("test_000200.textproto"));

INSTANTIATE_TEST_SUITE_P(BasicStereoLPCM, TestVector,
                         testing::Values("test_000003.textproto",
                                         "test_000201.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer3_1_2, TestVector,
                         testing::Values("test_000202.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer5_1_0, TestVector,
                         testing::Values("test_000203.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer5_1_2, TestVector,
                         testing::Values("test_000204.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer5_1_4, TestVector,
                         testing::Values("test_000205.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer7_1_0, TestVector,
                         testing::Values("test_000206.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer7_1_2, TestVector,
                         testing::Values("test_000207.textproto"));

// `test_000208` and `test_000211` are functionally identical.
INSTANTIATE_TEST_SUITE_P(LPCMOneLayer7_1_4, TestVector,
                         testing::Values("test_000208.textproto",
                                         "test_000211.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer7_1_4DemixingParamDefinition, TestVector,
                         testing::Values("test_000209.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMOneLayer7_1_4DemixingParameterBlocks, TestVector,
                         testing::Values("test_000210.textproto"));

INSTANTIATE_TEST_SUITE_P(BasicMonoOpus, TestVector,
                         testing::Values("test_000212.textproto"));

INSTANTIATE_TEST_SUITE_P(BasicStereoOpus, TestVector,
                         testing::Values("test_000213.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer3_1_2, TestVector,
                         testing::Values("test_000214.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer5_1_0, TestVector,
                         testing::Values("test_000215.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer5_1_2, TestVector,
                         testing::Values("test_000216.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer5_1_4, TestVector,
                         testing::Values("test_000217.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer7_1_0, TestVector,
                         testing::Values("test_000218.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer7_1_2, TestVector,
                         testing::Values("test_000219.textproto"));

// `test_000220` and `test_000223` are functionally identical.
INSTANTIATE_TEST_SUITE_P(OpusOneLayer7_1_4, TestVector,
                         testing::Values("test_000220.textproto",
                                         "test_000223.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer7_1_4DemixingParamDefinition, TestVector,
                         testing::Values("test_000221.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusOneLayer7_1_4DemixingParameterBlocks, TestVector,
                         testing::Values("test_000222.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMTwoLayer5_1_2, TestVector,
                         testing::Values("test_000224.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMThreeLayer7_1_4, TestVector,
                         testing::Values("test_000225.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMTwoLayer7_1_4, TestVector,
                         testing::Values("test_000226.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTwoLayer5_1_2ReconGain, TestVector,
                         testing::Values("test_000227.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusThreeLayer7_1_4ReconGain, TestVector,
                         testing::Values("test_000228.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTwoLayer7_1_4ReconGain, TestVector,
                         testing::Values("test_000229.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusThreeLayer5_1ReconGain, TestVector,
                         testing::Values("test_000230.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMBigEndian32Bit48kHz, TestVector,
                         testing::Values("test_000231.textproto"));

// ---- Test Set 3 -----

INSTANTIATE_TEST_SUITE_P(LPCMFOAStereoMix, TestVector,
                         testing::Values("test_000300.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMSOAStereoMix, TestVector,
                         testing::Values("test_000301.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMTOAStereoMix, TestVector,
                         testing::Values("test_000302.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusFOAStereoMix, TestVector,
                         testing::Values("test_000303.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusSOAStereoMix, TestVector,
                         testing::Values("test_000304.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusTOAStereoMix, TestVector,
                         testing::Values("test_000305.textproto"));

// ---- Test Set 4 -----

INSTANTIATE_TEST_SUITE_P(LPCMStereoStereoMix, TestVector,
                         testing::Values("test_000400.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMStereo3_1_2Mix, TestVector,
                         testing::Values("test_000401.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMStereo5_1Mix, TestVector,
                         testing::Values("test_000402.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusStereoStereoMix, TestVector,
                         testing::Values("test_000403.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusStereo3_1_2Mix, TestVector,
                         testing::Values("test_000404.textproto"));

INSTANTIATE_TEST_SUITE_P(OpusStereo5_1Mix, TestVector,
                         testing::Values("test_000405.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMStereoLinearMixGain, TestVector,
                         testing::Values("test_000406.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMStereoStereoMixBezierGain, TestVector,
                         testing::Values("test_000407.textproto"));

INSTANTIATE_TEST_SUITE_P(LPCMStereoStereoMixTwoSubblocks, TestVector,
                         testing::Values("test_000408.textproto"));

INSTANTIATE_TEST_SUITE_P(TwoMixPresentations, TestVector,
                         testing::Values("test_000409.textproto"));

// ---- Test Set 5 -----

INSTANTIATE_TEST_SUITE_P(FoaMonoMixedOrder, TestVector,
                         testing::Values("test_000500.textproto"));

INSTANTIATE_TEST_SUITE_P(ReservedDescriptorObu, TestVector,
                         testing::Values("test_000501.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidNumSubMixes, TestVector,
                         testing::Values("test_000502.textproto"));

INSTANTIATE_TEST_SUITE_P(LayoutExtension, TestVector,
                         testing::Values("test_000503.textproto"));

// ---- Test Set 6 -----

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6And3_0Ch, TestVector,
                         testing::Values("test_000600.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndTop4Ch, TestVector,
                         testing::Values("test_000601.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndTop6Ch, TestVector,
                         testing::Values("test_000602.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndLFE, TestVector,
                         testing::Values("test_000603.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoS, TestVector,
                         testing::Values("test_000604.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoSS, TestVector,
                         testing::Values("test_000605.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoRS, TestVector,
                         testing::Values("test_000606.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoTF, TestVector,
                         testing::Values("test_000607.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoTB, TestVector,
                         testing::Values("test_000608.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayout9_1_6AndStereoF, TestVector,
                         testing::Values("test_000609.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfZerothOrderAmbisonicsAndTop4Ch, TestVector,
                         testing::Values("test_000610.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFirstOrderAmbisonicsAndStereoF, TestVector,
                         testing::Values("test_000611.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfSecondOrderAmbisonicsAndStereoSi, TestVector,
                         testing::Values("test_000612.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfThirdOrderAmbisonicsAndStereoTpSi, TestVector,
                         testing::Values("test_000613.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAnd3_0Ch, TestVector,
                         testing::Values("test_000614.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndTop4Ch, TestVector,
                         testing::Values("test_000615.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndTop6Ch, TestVector,
                         testing::Values("test_000616.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndLFE, TestVector,
                         testing::Values("test_000617.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoS, TestVector,
                         testing::Values("test_000618.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoSS, TestVector,
                         testing::Values("test_000619.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoRS, TestVector,
                         testing::Values("test_000620.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoTF, TestVector,
                         testing::Values("test_000621.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoTB, TestVector,
                         testing::Values("test_000622.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfFourthOrderAmbisonicsAndStereoF, TestVector,
                         testing::Values("test_000623.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4And3_0Ch, TestVector,
                         testing::Values("test_000624.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndTop4Ch, TestVector,
                         testing::Values("test_000625.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndLFE, TestVector,
                         testing::Values("test_000626.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndStereoSS, TestVector,
                         testing::Values("test_000627.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndStereoRS, TestVector,
                         testing::Values("test_000628.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndStereoTF, TestVector,
                         testing::Values("test_000629.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndStereoTB, TestVector,
                         testing::Values("test_000630.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf5_1_4AndStereoS, TestVector,
                         testing::Values("test_000631.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfThirdOrderAmbisonicsAndLFE, TestVector,
                         testing::Values("test_000632.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfThirdOrderAmbisonicsAndTop6Ch, TestVector,
                         testing::Values("test_000633.textproto"));

// ---- Test Set 7 -----

INSTANTIATE_TEST_SUITE_P(MixOfThreeAudioElementsWithTwentyEightChannels,
                         TestVector, testing::Values("test_000700.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfThreeAudioElements, TestVector,
                         testing::Values("test_000701.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayoutsToCompose7_1_4, TestVector,
                         testing::Values("test_000702.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfExpandedLayoutsToCompose9_1_6, TestVector,
                         testing::Values("test_000703.textproto"));

INSTANTIATE_TEST_SUITE_P(OneMixPresentationWithContentLanguageTag, TestVector,
                         testing::Values("test_000704.textproto"));

INSTANTIATE_TEST_SUITE_P(
    SeveralMixPresentationsWithContentLanguageTagChannelBased, TestVector,
    testing::Values("test_000705.textproto"));

INSTANTIATE_TEST_SUITE_P(
    SeveralMixPresentationsWithContentLanguageTagAmbisonicsBased, TestVector,
    testing::Values("test_000706.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOfTwentyEightAudioElements, TestVector,
                         testing::Values("test_000707.textproto"));

INSTANTIATE_TEST_SUITE_P(MixOf7_1_4AndThirdOrderAmbisonics, TestVector,
                         testing::Values("test_000708.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidWithProfile255, TestVector,
                         testing::Values("test_000709.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidWithMoreThanTwentyEightAudioElements,
                         TestVector, testing::Values("test_000710.textproto"));

INSTANTIATE_TEST_SUITE_P(InvalidWithMoreThanTwentyEightChannels, TestVector,
                         testing::Values("test_000711.textproto"));

INSTANTIATE_TEST_SUITE_P(BaseEnhancedProfileWithTemporalUnitObus, TestVector,
                         testing::Values("test_000712.textproto"));

INSTANTIATE_TEST_SUITE_P(BaseAdvancedTwoLpcmCodecConfigs, TestVector,
                         testing::Values("test_000845.textproto"));

// TODO(b/308385831): Add more tests.

}  // namespace
}  // namespace iamf_tools
