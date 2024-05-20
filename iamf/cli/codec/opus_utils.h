#ifndef CLI_CODEC_OPUS_UTILS_H_
#define CLI_CODEC_OPUS_UTILS_H_

#include "absl/status/status.h"

namespace iamf_tools {

/*!\brief Translates from Opus error codes to Abseil standard error codes.*/
absl::StatusCode OpusErrorCodeToAbslStatusCode(int opus_error_code);

}  // namespace iamf_tools

#endif  // CLI_CODEC_OPUS_UTILS_H_
