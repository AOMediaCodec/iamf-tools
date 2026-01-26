/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/proto_conversion/proto_to_obu/metadata_obu_generator.h"

#include <cstdint>
#include <list>
#include <variant>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/metadata_obu.pb.h"
#include "iamf/obu/metadata_obu.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::iamf_tools_cli_proto::MetadataObuMetadata;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Not;

TEST(MetadataObuGeneratorTest, EmptyInputGeneratesEmptyOutput) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;
  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), IsOk());
  EXPECT_TRUE(metadata_obus.empty());
}

TEST(MetadataObuGeneratorTest, FailsWhenNoMetadataIsSet) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;
  metadata_obu_metadata.Add();
  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), Not(IsOk()));
}

TEST(MetadataObuGeneratorTest, GeneratesMetadataItuT35) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;
  auto* metadata = metadata_obu_metadata.Add();
  auto* itu_t_t35 = metadata->mutable_metadata_itu_t_t35();
  itu_t_t35->set_itu_t_t35_country_code(1);
  itu_t_t35->set_itu_t_t35_payload_bytes("abc");

  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), IsOk());
  EXPECT_EQ(metadata_obus.size(), 1);

  auto generated_metadata =
      std::get_if<MetadataITUTT35>(&metadata_obus.front().GetMetadataVariant());
  ASSERT_THAT(generated_metadata, Not(testing::IsNull()));

  EXPECT_EQ(generated_metadata->itu_t_t35_country_code, 1);
  EXPECT_FALSE(
      generated_metadata->itu_t_t35_country_code_extension_byte.has_value());
  EXPECT_EQ(generated_metadata->itu_t_t35_payload_bytes,
            std::vector<uint8_t>({'a', 'b', 'c'}));
}

TEST(MetadataObuGeneratorTest, GeneratesMetadataItuT35WithExtension) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;
  auto* metadata = metadata_obu_metadata.Add();
  auto* itu_t_t35 = metadata->mutable_metadata_itu_t_t35();
  itu_t_t35->set_itu_t_t35_country_code(0xff);
  itu_t_t35->set_itu_t_t35_country_code_extension_byte(2);
  itu_t_t35->set_itu_t_t35_payload_bytes("abc");

  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), IsOk());
  EXPECT_EQ(metadata_obus.size(), 1);

  auto generated_metadata =
      std::get_if<MetadataITUTT35>(&metadata_obus.front().GetMetadataVariant());
  ASSERT_THAT(generated_metadata, Not(testing::IsNull()));

  EXPECT_EQ(generated_metadata->itu_t_t35_country_code, 0xff);
  EXPECT_EQ(generated_metadata->itu_t_t35_country_code_extension_byte, 2);
  EXPECT_EQ(generated_metadata->itu_t_t35_payload_bytes,
            std::vector<uint8_t>({'a', 'b', 'c'}));
}

TEST(MetadataObuGeneratorTest, GeneratesMetadataIamfTags) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;
  auto* metadata = metadata_obu_metadata.Add();
  auto* iamf_tags = metadata->mutable_metadata_iamf_tags();
  auto* tag = iamf_tags->add_tags();
  tag->set_name("key");
  tag->set_value("value");

  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), IsOk());
  EXPECT_EQ(metadata_obus.size(), 1);

  auto generated_metadata = std::get_if<MetadataIamfTags>(
      &metadata_obus.front().GetMetadataVariant());
  ASSERT_THAT(generated_metadata, Not(testing::IsNull()));
  EXPECT_THAT(generated_metadata->tags, ElementsAre(FieldsAre("key", "value")));
}

TEST(MetadataObuGeneratorTest, GeneratesMultipleMetadataObus) {
  google::protobuf::RepeatedPtrField<MetadataObuMetadata> metadata_obu_metadata;

  // Add a MetadataITUTT35 OBU.
  auto* metadata_itu_t_t35 =
      metadata_obu_metadata.Add()->mutable_metadata_itu_t_t35();
  metadata_itu_t_t35->set_itu_t_t35_country_code(1);
  metadata_itu_t_t35->set_itu_t_t35_payload_bytes("abc");

  // Add a MetadataIamfTags OBU.
  auto* iamf_tags = metadata_obu_metadata.Add()->mutable_metadata_iamf_tags();
  auto* tag = iamf_tags->add_tags();
  tag->set_name("key");
  tag->set_value("value");

  MetadataObuGenerator generator(metadata_obu_metadata);
  std::list<MetadataObu> metadata_obus;

  EXPECT_THAT(generator.Generate(metadata_obus), IsOk());
  EXPECT_EQ(metadata_obus.size(), 2);

  // Validate the first OBU.
  auto generated_metadata_itu_t_t35 =
      std::get_if<MetadataITUTT35>(&metadata_obus.front().GetMetadataVariant());
  ASSERT_THAT(generated_metadata_itu_t_t35, Not(testing::IsNull()));
  EXPECT_EQ(generated_metadata_itu_t_t35->itu_t_t35_country_code, 1);
  EXPECT_FALSE(generated_metadata_itu_t_t35
                   ->itu_t_t35_country_code_extension_byte.has_value());
  EXPECT_EQ(generated_metadata_itu_t_t35->itu_t_t35_payload_bytes,
            std::vector<uint8_t>({'a', 'b', 'c'}));

  // Validate the second OBU.
  auto generated_metadata_iamf_tags =
      std::get_if<MetadataIamfTags>(&metadata_obus.back().GetMetadataVariant());
  ASSERT_THAT(generated_metadata_iamf_tags, Not(testing::IsNull()));
  EXPECT_THAT(generated_metadata_iamf_tags->tags,
              ElementsAre(FieldsAre("key", "value")));
}

}  // namespace
}  // namespace iamf_tools
