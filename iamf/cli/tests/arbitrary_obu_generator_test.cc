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
#include "iamf/cli/arbitrary_obu_generator.h"

#include <cstdint>
#include <list>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/obu_header.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

class ArbitraryObuGeneratorTest : public testing::Test {
 public:
  ArbitraryObuGeneratorTest() {}

  void InitAndTestGenerate() {
    // Generate the OBUs.
    std::list<ArbitraryObu> output_obus;
    ArbitraryObuGenerator generator(arbitrary_obu_metadata_);
    EXPECT_EQ(generator.Generate(output_obus).code(),
              expected_generate_status_code_);
    EXPECT_EQ(output_obus, expected_obus_);
  }

 protected:
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::ArbitraryObuMetadata>
      arbitrary_obu_metadata_;

  absl::StatusCode expected_generate_status_code_ = absl::StatusCode::kOk;

  std::list<ArbitraryObu> expected_obus_;
};

TEST_F(ArbitraryObuGeneratorTest, NoArbitraryObuObus) {
  arbitrary_obu_metadata_.Clear();
  InitAndTestGenerate();
}

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
  InitAndTestGenerate();
}

TEST_F(ArbitraryObuGeneratorTest, InsertionHookAfterIaSequenceHeader) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_AFTER_IA_SEQUENCE_HEADER
        obu_type: OBU_IA_RESERVED_24
      )pb",
      arbitrary_obu_metadata_.Add()));

  expected_obus_.emplace_back(
      kObuIaReserved24, ObuHeader(), std::vector<uint8_t>{},
      ArbitraryObu::kInsertionHookAfterIaSequenceHeader);
  InitAndTestGenerate();
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
  InitAndTestGenerate();
}

TEST_F(ArbitraryObuGeneratorTest, InvalidInsertionHook) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insertion_hook: INSERTION_HOOK_INVALID
        obu_type: OBU_IA_RESERVED_24
        obu_header {
          obu_redundant_copy: false
          obu_trimming_status_flag: false
          obu_extension_flag: false
        }
        payload: ""
      )pb",
      arbitrary_obu_metadata_.Add()));
  expected_generate_status_code_ = absl::StatusCode::kInvalidArgument;

  InitAndTestGenerate();
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
  expected_generate_status_code_ = absl::StatusCode::kInvalidArgument;

  InitAndTestGenerate();
}

}  // namespace
}  // namespace iamf_tools
