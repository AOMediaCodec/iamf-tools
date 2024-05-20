#include "iamf/cli/codec/opus_utils.h"

#include "absl/status/status.h"
#include "include/opus_defines.h"

namespace iamf_tools {

absl::StatusCode OpusErrorCodeToAbslStatusCode(int opus_error_code) {
  switch (opus_error_code) {
    case OPUS_OK:
      return absl::StatusCode::kOk;
    case OPUS_BAD_ARG:
      return absl::StatusCode::kInvalidArgument;
    case OPUS_BUFFER_TOO_SMALL:
    case OPUS_INVALID_STATE:
      return absl::StatusCode::kFailedPrecondition;
    case OPUS_INTERNAL_ERROR:
      return absl::StatusCode::kInternal;
    case OPUS_INVALID_PACKET:
      return absl::StatusCode::kDataLoss;
    case OPUS_UNIMPLEMENTED:
      return absl::StatusCode::kUnimplemented;
    case OPUS_ALLOC_FAIL:
      return absl::StatusCode::kResourceExhausted;
    default:
      return absl::StatusCode::kUnknown;
  }
}

}  // namespace iamf_tools
