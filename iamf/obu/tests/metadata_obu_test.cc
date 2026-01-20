#include "iamf/obu/metadata_obu.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

TEST(CreateWithMetadata, ITUTT35) {
  MetadataObu obu = MetadataObu::Create(ObuHeader(), MetadataITUTT35());
  EXPECT_EQ(obu.GetMetadataType(), kMetadataTypeITUT_T35);
}

TEST(CreateWithMetadata, IamfTags) {
  MetadataObu obu = MetadataObu::Create(ObuHeader(), MetadataIamfTags());
  EXPECT_EQ(obu.GetMetadataType(), kMetadataTypeIamfTags);
}

TEST(CreateFromBuffer, Unimplemented) {
  std::vector<uint8_t> buffer_data;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(buffer_data);

  EXPECT_THAT(MetadataObu::CreateFromBuffer(ObuHeader(), 0, *rb), Not(IsOk()));
}

TEST(ValidateAndWrite, Unimplemented) {
  MetadataObu obu = MetadataObu::Create(ObuHeader(), MetadataITUTT35());
  WriteBitBuffer wb(0);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
