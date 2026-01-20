#include "iamf/obu/metadata_obu.h"

#include <array>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr auto kIamfTagsHeader =
    std::to_array<uint8_t>({kObuIaMetadata << 3, 22});
constexpr auto kITUTT35Header =
    std::to_array<uint8_t>({kObuIaMetadata << 3, 7});

TEST(CreateWithMetadata, ITUTT35) {
  MetadataObu obu = MetadataObu::Create(ObuHeader(), MetadataITUTT35());
  EXPECT_EQ(obu.GetMetadataType(), kMetadataTypeITUT_T35);
}

TEST(CreateWithMetadata, IamfTags) {
  MetadataObu obu = MetadataObu::Create(ObuHeader(), MetadataIamfTags());
  EXPECT_EQ(obu.GetMetadataType(), kMetadataTypeIamfTags);
}

TEST(CreateFromBuffer, IamfTags) {
  std::vector<uint8_t> buffer_data = {kMetadataTypeIamfTags,
                                      /*num_tags=*/2,
                                      /*tag_name_1=*/
                                      't', 'a', 'g', '1', '\0',
                                      /*tag_value_1=*/
                                      'v', 'a', 'l', '1', '\0',
                                      /*tag_name_2=*/
                                      't', 'a', 'g', '2', '\0',
                                      /*tag_value_2=*/
                                      'v', 'a', 'l', '2', '\0'};
  const int64_t payload_size = buffer_data.size();
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(buffer_data);

  auto obu = MetadataObu::CreateFromBuffer(ObuHeader(), payload_size, *rb);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetMetadataType(), kMetadataTypeIamfTags);
  auto& metadata_iamf_tags =
      std::get<MetadataIamfTags>(obu->GetMetadataVariant());
  EXPECT_EQ(metadata_iamf_tags.tags.size(), 2);
  EXPECT_EQ(metadata_iamf_tags.tags[0].tag_name, "tag1");
  EXPECT_EQ(metadata_iamf_tags.tags[0].tag_value, "val1");
  EXPECT_EQ(metadata_iamf_tags.tags[1].tag_name, "tag2");
  EXPECT_EQ(metadata_iamf_tags.tags[1].tag_value, "val2");
}

TEST(CreateFromBuffer, ITUTT35CountryCode0xFF) {
  std::vector<uint8_t> buffer_data = {kMetadataTypeITUT_T35,
                                      /*itu_t_t35_country_code=*/
                                      0xFF,
                                      /*itu_t_t35_country_code_extension_byte=*/
                                      0x02,
                                      /*itu_t_t35_payload_bytes=*/
                                      0x03, 0x04, 0x05, 0x06, 0x07};
  const int64_t payload_size = buffer_data.size();
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(buffer_data);
  auto obu = MetadataObu::CreateFromBuffer(ObuHeader(), payload_size, *rb);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetMetadataType(), kMetadataTypeITUT_T35);
  auto& metadata_itu_t_t35 =
      std::get<MetadataITUTT35>(obu->GetMetadataVariant());
  EXPECT_EQ(metadata_itu_t_t35.itu_t_t35_country_code, 0xFF);
  EXPECT_EQ(metadata_itu_t_t35.itu_t_t35_country_code_extension_byte, 0x02);
  EXPECT_THAT(metadata_itu_t_t35.itu_t_t35_payload_bytes,
              testing::ElementsAre(0x03, 0x04, 0x05, 0x06, 0x07));
}

TEST(CreateFromBuffer, ITUTT35) {
  std::vector<uint8_t> buffer_data = {kMetadataTypeITUT_T35,
                                      /*itu_t_t35_country_code=*/
                                      0x01,
                                      /*itu_t_t35_payload_bytes=*/
                                      0x03, 0x04, 0x05, 0x06, 0x07};
  const int64_t payload_size = buffer_data.size();
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(buffer_data);

  auto obu = MetadataObu::CreateFromBuffer(ObuHeader(), payload_size, *rb);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetMetadataType(), kMetadataTypeITUT_T35);
  auto& metadata_itu_t_t35 =
      std::get<MetadataITUTT35>(obu->GetMetadataVariant());
  EXPECT_EQ(metadata_itu_t_t35.itu_t_t35_country_code, 0x01);
  EXPECT_FALSE(
      metadata_itu_t_t35.itu_t_t35_country_code_extension_byte.has_value());
  EXPECT_THAT(metadata_itu_t_t35.itu_t_t35_payload_bytes,
              testing::ElementsAre(0x03, 0x04, 0x05, 0x06, 0x07));
}

TEST(ValidateAndWrite, IamfTags) {
  MetadataIamfTags metadata_iamf_tags;
  metadata_iamf_tags.tags.reserve(2);
  metadata_iamf_tags.tags.push_back(
      MetadataIamfTags::Tag{.tag_name = "tag1", .tag_value = "val1"});
  metadata_iamf_tags.tags.push_back(
      MetadataIamfTags::Tag{.tag_name = "tag2", .tag_value = "val2"});
  MetadataObu obu = MetadataObu::Create(ObuHeader(), metadata_iamf_tags);
  WriteBitBuffer wb(0);
  std::vector<uint8_t> expected_payload_bytes = {kMetadataTypeIamfTags,
                                                 /*num_tags=*/2,
                                                 /*tag_name_1=*/
                                                 't', 'a', 'g', '1', '\0',
                                                 /*tag_value_1=*/
                                                 'v', 'a', 'l', '1', '\0',
                                                 /*tag_name_2=*/
                                                 't', 'a', 'g', '2', '\0',
                                                 /*tag_value_2=*/
                                                 'v', 'a', 'l', '2', '\0'};

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());
  ValidateObuWriteResults(wb, absl::MakeConstSpan(kIamfTagsHeader),
                          absl::MakeConstSpan(expected_payload_bytes));
}

TEST(ValidateAndWrite, ITUTT35) {
  MetadataITUTT35 metadata_itu_t_t35;
  metadata_itu_t_t35.itu_t_t35_country_code = 0x01;
  metadata_itu_t_t35.itu_t_t35_payload_bytes = {0x03, 0x04, 0x05, 0x06, 0x07};
  MetadataObu obu = MetadataObu::Create(ObuHeader(), metadata_itu_t_t35);
  WriteBitBuffer wb(0);
  std::vector<uint8_t> expected_payload_bytes = {kMetadataTypeITUT_T35,
                                                 /*itu_t_t35_country_code=*/
                                                 0x01,
                                                 /*itu_t_t35_payload_bytes=*/
                                                 0x03, 0x04, 0x05, 0x06, 0x07};

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());
  ValidateObuWriteResults(wb, absl::MakeConstSpan(kITUTT35Header),
                          absl::MakeConstSpan(expected_payload_bytes));
}

}  // namespace
}  // namespace iamf_tools
