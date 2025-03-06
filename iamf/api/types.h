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
enum class OutputLayout {
  // ITU-R B.S. 2051-3 sound system A (0+2+0), commonly known as Stereo.
  kItu2051_SoundSystemA_0_2_0 = 0,
  // ITU-R B.S. 2051-3 sound system B (0+5+0), commonly known as 5.1.
  kItu2051_SoundSystemB_0_5_0 = 1,
  // ITU-R B.S. 2051-3 sound system C (2+5+0), commonly known as 5.1.2.
  kItu2051_SoundSystemC_2_5_0 = 2,
  // ITU-R B.S. 2051-3 sound system D (4+5+0), commonly known as 5.1.4.
  kItu2051_SoundSystemD_4_5_0 = 3,
  // ITU-R B.S. 2051-3 sound system E (4+5+1).
  kItu2051_SoundSystemE_4_5_1 = 4,
  // ITU-R B.S. 2051-3 sound system F (3+7+0).
  kItu2051_SoundSystemF_3_7_0 = 5,
  // ITU-R B.S. 2051-3 sound system G (4+9+0).
  kItu2051_SoundSystemG_4_9_0 = 6,
  // ITU-R B.S. 2051-3 sound system H (9+10+3).
  kItu2051_SoundSystemH_9_10_3 = 7,
  // ITU-R B.S. 2051-3 sound system I (0+7+0), commonly known as 7.1.
  kItu2051_SoundSystemI_0_7_0 = 8,
  // ITU-R B.S. 2051-3 sound system J (4+7+0), commonly known as 7.1.4.
  kItu2051_SoundSystemJ_4_7_0 = 9,
  // IAMF extension 7.1.2.
  kIAMF_SoundSystemExtension_2_7_0 = 10,
  // IAMF extension 3.1.2.
  kIAMF_SoundSystemExtension_2_3_0 = 11,
  // Mono.
  kIAMF_SoundSystemExtension_0_1_0 = 12,
  // IAMF Extension 9.1.6.
  kIAMF_SoundSystemExtension_6_9_0 = 13,
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
