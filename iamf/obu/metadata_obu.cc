#include "iamf/obu/metadata_obu.h"

#include <cstdint>
#include <utility>
#include <variant>

#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

MetadataObu MetadataObu::Create(const ObuHeader& header,
                                MetadataVariant metadata_variant) {
  MetadataType metadata_type = std::visit(
      absl::Overload{
          [](const MetadataITUTT35& arg) { return kMetadataTypeITUT_T35; },
          [](const MetadataIamfTags& arg) { return kMetadataTypeIamfTags; }},
      metadata_variant);
  return MetadataObu(header, metadata_type, std::move(metadata_variant));
}

absl::StatusOr<MetadataObu> MetadataObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  return absl::UnimplementedError("Not implemented");
}

void MetadataObu::PrintObu() const {}

absl::Status MetadataObu::ValidateAndWritePayload(WriteBitBuffer& wb) const {
  return absl::UnimplementedError("Not implemented");
}

absl::Status MetadataObu::ReadAndValidatePayloadDerived(int64_t payload_size,
                                                        ReadBitBuffer& rb) {
  return absl::UnimplementedError("Not implemented");
}

}  // namespace iamf_tools
