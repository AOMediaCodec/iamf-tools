#include <sys/types.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "fuzztest/fuzztest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

void ReadUnsignedLiteral64NoUndefinedBehavior(std::vector<uint8_t> data,
                                              int num_bits) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  uint64_t read_data;
  absl::Status status = rb->ReadUnsignedLiteral(num_bits, read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadUnsignedLiteral64NoUndefinedBehavior);

void ReadUnsignedLiteral32NoUndefinedBehavior(std::vector<uint8_t> data,
                                              int num_bits) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  uint32_t read_data;
  absl::Status status = rb->ReadUnsignedLiteral(num_bits, read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadUnsignedLiteral32NoUndefinedBehavior);

void ReadUnsignedLiteral16NoUndefinedBehavior(std::vector<uint8_t> data,
                                              int num_bits) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  uint16_t read_data;
  absl::Status status = rb->ReadUnsignedLiteral(num_bits, read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadUnsignedLiteral16NoUndefinedBehavior);

void ReadUnsignedLiteral8NoUndefinedBehavior(std::vector<uint8_t> data,
                                             int num_bits) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  uint8_t read_data;
  absl::Status status = rb->ReadUnsignedLiteral(num_bits, read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadUnsignedLiteral8NoUndefinedBehavior);

void ReadSigned16NoUndefinedBehavior(std::vector<uint8_t> data) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  int16_t read_data;
  absl::Status status = rb->ReadSigned16(read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadSigned16NoUndefinedBehavior);

void ReadStringNoUndefinedBehavior(std::vector<uint8_t> data) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  std::string read_data;
  absl::Status status = rb->ReadString(read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadStringNoUndefinedBehavior);

void ReadULeb128NoUndefinedBehavior(std::vector<uint8_t> data) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  DecodedUleb128 read_data;
  absl::Status status = rb->ReadULeb128(read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadULeb128NoUndefinedBehavior);

void ReadIso14496_1ExpandedNoUndefinedBehavior(std::vector<uint8_t> data,
                                               uint32_t max_class_size) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  uint32_t read_data;
  absl::Status status = rb->ReadIso14496_1Expanded(max_class_size, read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadIso14496_1ExpandedNoUndefinedBehavior);

void ReadUint8SpanNoUndefinedBehavior(std::vector<uint8_t> data) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  std::vector<uint8_t> read_data(data.size());
  absl::Span<uint8_t> span(read_data);
  absl::Status status = rb->ReadUint8Span(span);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadUint8SpanNoUndefinedBehavior);

void ReadBooleanNoUndefinedBehavior(std::vector<uint8_t> data) {
  std::unique_ptr<MemoryBasedReadBitBuffer> rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(256, absl::MakeConstSpan(data));
  bool read_data;
  absl::Status status = rb->ReadBoolean(read_data);
}

FUZZ_TEST(ReadBitBufferFuzzTest, ReadBooleanNoUndefinedBehavior);

}  // namespace
}  // namespace iamf_tools
