/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef IA_H_
#define IA_H_

#include <cstdint>

namespace iamf_tools {

/*!\brief An 8-bit enum for the profile. */
enum class ProfileVersion : uint8_t {
  kIamfSimpleProfile = 0,
  kIamfBaseProfile = 1,
};

/*!\brief The maximum length of an IAMF string in bytes.
 *
 * The spec limits the length of a string to 128 bytes including the
 * null terminator ('\0').
 */
inline constexpr int kIamfMaxStringSize = 128;

/*!\brief A decoded `leb128` in IAMF.  */
typedef uint32_t DecodedUleb128;

/*!\brief A decoded `sleb128` in IAMF.  */
typedef int32_t DecodedSleb128;

/*!\brief A `string` as defined by the IAMF spec.
 *
 * The IAMF spec requires this is null terminated and at most 128 bytes.
 */
typedef char IamfString[kIamfMaxStringSize];

// For propagating errors when calling a function. Beware that defining
// `NO_CHECK_ERROR` is not thoroughly tested and may result in unexpected
// behavior.
#ifdef NO_CHECK_ERROR
#define RETURN_IF_NOT_OK(...)    \
  do {                           \
    (__VA_ARGS__).IgnoreError(); \
  } while (0)
#else
#define RETURN_IF_NOT_OK(...)             \
  do {                                    \
    absl::Status _status = (__VA_ARGS__); \
    if (!_status.ok()) return _status;    \
  } while (0)
#endif

/*!\brief A 5-bit enum for the type of OBU. */
enum ObuType : uint8_t {
  kObuIaCodecConfig = 0,
  kObuIaAudioElement = 1,
  kObuIaMixPresentation = 2,
  kObuIaParameterBlock = 3,
  kObuIaTemporalDelimiter = 4,
  kObuIaAudioFrame = 5,
  kObuIaAudioFrameId0 = 6,
  kObuIaAudioFrameId1 = 7,
  kObuIaAudioFrameId2 = 8,
  kObuIaAudioFrameId3 = 9,
  kObuIaAudioFrameId4 = 10,
  kObuIaAudioFrameId5 = 11,
  kObuIaAudioFrameId6 = 12,
  kObuIaAudioFrameId7 = 13,
  kObuIaAudioFrameId8 = 14,
  kObuIaAudioFrameId9 = 15,
  kObuIaAudioFrameId10 = 16,
  kObuIaAudioFrameId11 = 17,
  kObuIaAudioFrameId12 = 18,
  kObuIaAudioFrameId13 = 19,
  kObuIaAudioFrameId14 = 20,
  kObuIaAudioFrameId15 = 21,
  kObuIaAudioFrameId16 = 22,
  kObuIaAudioFrameId17 = 23,
  kObuIaReserved24 = 24,
  kObuIaReserved25 = 25,
  kObuIaReserved26 = 26,
  kObuIaReserved27 = 27,
  kObuIaReserved28 = 28,
  kObuIaReserved29 = 29,
  kObuIaReserved30 = 30,
  kObuIaSequenceHeader = 31,
};

}  // namespace iamf_tools

#endif  // IA_H_
