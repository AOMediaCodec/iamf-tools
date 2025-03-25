
#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "fuzztest/fuzztest.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

void WriteReadString(const std::string& data) {
  WriteBitBuffer wb(0);
  auto write_status = wb.WriteString(data);

  if (write_status.ok()) {
    std::vector<uint8_t> source_data = wb.bit_buffer();
    std::unique_ptr<MemoryBasedReadBitBuffer> rb =
        MemoryBasedReadBitBuffer::CreateFromSpan(
            absl::MakeConstSpan(source_data));

    std::string read_data;
    EXPECT_THAT(rb->ReadString(read_data), IsOk());

    EXPECT_EQ(read_data, data);
  }
}

FUZZ_TEST(WriteReadFuzzTest, WriteReadString);

void WriteReadUint8Span(const std::vector<uint8_t>& data) {
  WriteBitBuffer wb(0);
  auto write_status = wb.WriteUint8Span(absl::MakeConstSpan(data));

  if (write_status.ok()) {
    std::vector<uint8_t> source_data = wb.bit_buffer();
    std::unique_ptr<MemoryBasedReadBitBuffer> rb =
        MemoryBasedReadBitBuffer::CreateFromSpan(
            absl::MakeConstSpan(source_data));

    std::vector<uint8_t> read_data(data.size());
    EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(read_data)), IsOk());

    EXPECT_EQ(read_data, data);
  }
}

FUZZ_TEST(WriteReadFuzzTest, WriteReadUint8Span);

}  // namespace
}  // namespace iamf_tools
