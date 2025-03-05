/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef API_DECODER_TYPES_H_
#define API_DECODER_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

namespace iamf_tools {
namespace api {

// TODO(b/339500539): Add support for other IAMF supported layouts
/*!\brief Determines the layout of the output file.
 *
 * Typically these correspond with `sound_system`s in the IAMF spec
 * (https://aomediacodec.github.io/iamf/#syntax-layout).
 */
enum OutputLayout {
  kOutputStereo,
};

/*!\brief Determines the format of the output file. */
enum OutputFileBitDepth {
  kBitDepthAutomatic,  // Automatically determine based on the bit-depth of
                       // the input file.
  kBitDepth16,
  kBitDepth24,
  kBitDepth32,
};

/*!\brief A unique identifier for a `MixPresentation` in the IAMF stream. */
using MixPresentationId = uint32_t;

/*!\brief A name:value tag describing a `MixPresentation` in the IAMF stream. */
struct MixPresentationTag {
  std::string tag_name;
  std::string tag_value;
};

/*!\brief Metadata that describes a mix presentation.
 *
 * Used by a user to determine which mix presentation they would like to
 * configure the decoder with.
 */
struct MixPresentationMetadata {
  MixPresentationId id;
  std::vector<MixPresentationTag> tags;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // API_DECODER_TYPES_H_
