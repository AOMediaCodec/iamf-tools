#include "iamf/cli/codec/aac_utils.h"

#include <string>

#include "absl/log/log.h"

// This symbol conflicts with `aacenc_lib.h` and `aacdecoder_lib.h`.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "libAACenc/include/aacenc_lib.h"

namespace iamf_tools {

absl::Status AacEncErrorToAbslStatus(AACENC_ERROR aac_error_code,
                                     const std::string& error_message) {
  absl::StatusCode status_code;
  switch (aac_error_code) {
    case AACENC_OK:
      return absl::OkStatus();
    case AACENC_INVALID_HANDLE:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_MEMORY_ERROR:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    case AACENC_UNSUPPORTED_PARAMETER:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_INVALID_CONFIG:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    case AACENC_INIT_ERROR:
    case AACENC_INIT_AAC_ERROR:
    case AACENC_INIT_SBR_ERROR:
    case AACENC_INIT_TP_ERROR:
    case AACENC_INIT_META_ERROR:
    case AACENC_INIT_MPS_ERROR:
      status_code = absl::StatusCode::kInternal;
      break;
    case AACENC_ENCODE_EOF:
      status_code = absl::StatusCode::kOutOfRange;
      break;
    case AACENC_ENCODE_ERROR:
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }

  return absl::Status(
      status_code,
      absl::StrCat(error_message, " AACENC_ERROR= ", aac_error_code));
}

}  // namespace iamf_tools
