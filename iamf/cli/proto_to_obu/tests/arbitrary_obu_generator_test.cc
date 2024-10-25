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
#include "iamf/cli/proto_to_obu/arbitrary_obu_generator.h"

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/obu_header.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

typedef ::google::protobuf::RepeatedPtrField<
    iamf_tools_cli_proto::ArbitraryObuMetadata>
    ArbitraryObuMetadatas;

constexpr int64_t kInsertionTick = 123;

void FillArbitraryObu(
    iamf_tools_cli_proto::ArbitraryObuMetadata* arbitrary_obu_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_BEFORE_DESCRIPTORS
        obu_type: OBU_IA_RESERVED_24
        obu_header {
          obu_redundant_copy: false
          obu_trimming_status_flag: false
          obu_extension_flag: false
        }
        payload: ""
      )pb",
      arbitrary_obu_metadata));
}

TEST(Generate, CopiesInsertionHookBeforeDescriptors) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_BEFORE_DESCRIPTORS);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookBeforeDescriptors);
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, std::nullopt);
}

TEST(Generate, CopiesInsertionHookAfterDescriptors) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_AFTER_DESCRIPTORS);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookAfterDescriptors);
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, std::nullopt);
}

TEST(Generate, CopiesInsertionHookAfterCodecConfigs) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_AFTER_CODEC_CONFIGS);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookAfterCodecConfigs);
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, std::nullopt);
}

TEST(Generate, InsertionTickDefaultsToZeroForTimeBasedInsertionHooks) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_BEFORE_PARAMETER_BLOCKS_AT_TICK);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookBeforeParameterBlocksAtTick);
  ASSERT_TRUE(arbitrary_obus.front().insertion_tick_.has_value());
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, 0);
}

TEST(Generate, CopiesInsertionTickForTimeBasedInsertionHooks) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_BEFORE_PARAMETER_BLOCKS_AT_TICK);
  arbitrary_obu_metadatas.at(0).set_insertion_tick(kInsertionTick);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookBeforeParameterBlocksAtTick);
  ASSERT_TRUE(arbitrary_obus.front().insertion_tick_.has_value());
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, kInsertionTick);
}

TEST(Generate, CopiesInsertionHookAfterParameterBlocksAtTime) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_AFTER_PARAMETER_BLOCKS_AT_TICK);
  arbitrary_obu_metadatas.at(0).set_insertion_tick(kInsertionTick);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick);
  ASSERT_TRUE(arbitrary_obus.front().insertion_tick_.has_value());
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, kInsertionTick);
}

TEST(Generate, CopiesInsertionHookAfterAudioFramesAtTime) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_AFTER_AUDIO_FRAMES_AT_TICK);
  arbitrary_obu_metadatas.at(0).set_insertion_tick(kInsertionTick);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().insertion_hook_,
            ArbitraryObu::kInsertionHookAfterAudioFramesAtTick);
  ASSERT_TRUE(arbitrary_obus.front().insertion_tick_.has_value());
  EXPECT_EQ(arbitrary_obus.front().insertion_tick_, kInsertionTick);
}

TEST(Generate, FailsOnInvalidInsertionHook) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_insertion_hook(
      iamf_tools_cli_proto::INSERTION_HOOK_INVALID);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;

  EXPECT_FALSE(generator.Generate(arbitrary_obus).ok());
  EXPECT_TRUE(arbitrary_obus.empty());
}

TEST(Generate, CopiesInvalidatesBitstreamFalse) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_invalidates_bitstream(false);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().invalidates_bitstream_, false);
}

TEST(Generate, CopiesInvalidatesBitstreamTrue) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas;
  FillArbitraryObu(arbitrary_obu_metadatas.Add());
  arbitrary_obu_metadatas.at(0).set_invalidates_bitstream(true);

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_EQ(arbitrary_obus.front().invalidates_bitstream_, true);
}

TEST(Generate, GeneratesEmptyListForEmptyInput) {
  ArbitraryObuMetadatas arbitrary_obu_metadatas = {};

  ArbitraryObuGenerator generator(arbitrary_obu_metadatas);
  std::list<ArbitraryObu> arbitrary_obus;
  EXPECT_THAT(generator.Generate(arbitrary_obus), IsOk());

  EXPECT_TRUE(arbitrary_obus.empty());
}

class ArbitraryObuGeneratorTest : public testing::Test {
 public:
  ArbitraryObuGeneratorTest() {}

  void InitAndTestGenerateExpectOk() {
    // Generate the OBUs.
    std::list<ArbitraryObu> output_obus;
    ArbitraryObuGenerator generator(arbitrary_obu_metadata_);
    EXPECT_THAT(generator.Generate(output_obus), IsOk());

    EXPECT_EQ(output_obus, expected_obus_);
  }

 protected:
  ArbitraryObuMetadatas arbitrary_obu_metadata_;

  std::list<ArbitraryObu> expected_obus_;
};

TEST_F(ArbitraryObuGeneratorTest, ReservedObu) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_BEFORE_DESCRIPTORS
        obu_type: OBU_IA_RESERVED_24
        obu_header {
          obu_redundant_copy: false
          obu_trimming_status_flag: false
          obu_extension_flag: false
        }
        payload: "abc"
      )pb",
      arbitrary_obu_metadata_.Add()));

  expected_obus_.emplace_back(kObuIaReserved24, ObuHeader(),
                              std::vector<uint8_t>({'a', 'b', 'c'}),
                              ArbitraryObu::kInsertionHookBeforeDescriptors);
  InitAndTestGenerateExpectOk();
}

TEST_F(ArbitraryObuGeneratorTest, ObuWithExtensionHeader) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_DESCRIPTORS
        obu_type: OBU_IA_SEQUENCE_HEADER
        obu_header {
          obu_redundant_copy: false
          obu_trimming_status_flag: false
          obu_extension_flag: true
          extension_header_size: 5
          extension_header_bytes: "extra"
        }
        payload: "iamf\x00\x00"
      )pb",
      arbitrary_obu_metadata_.Add()));

  expected_obus_.emplace_back(
      kObuIaSequenceHeader,
      ObuHeader{.obu_extension_flag = true,
                .extension_header_size = 5,
                .extension_header_bytes = {'e', 'x', 't', 'r', 'a'}},
      std::vector<uint8_t>({'i', 'a', 'm', 'f', '\0', '\0'}),
      ArbitraryObu::kInsertionHookAfterDescriptors);
  InitAndTestGenerateExpectOk();
}

TEST_F(ArbitraryObuGeneratorTest, InvalidObuType) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_DESCRIPTORS
        obu_type: OBU_IA_INVALID
        obu_header {
          obu_redundant_copy: false
          obu_trimming_status_flag: false
          obu_extension_flag: false
        }
        payload: ""
      )pb",
      arbitrary_obu_metadata_.Add()));
  std::list<ArbitraryObu> output_obus;
  ArbitraryObuGenerator generator(arbitrary_obu_metadata_);

  EXPECT_FALSE(generator.Generate(output_obus).ok());
}

}  // namespace
}  // namespace iamf_tools
