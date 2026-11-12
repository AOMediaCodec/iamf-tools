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
#ifndef INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_
#define INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

namespace iamf_tools {
namespace api {

// Audio channels for this audio element, mapped by a serialized
// `ChannelLabelMessage` protocol buffer.
using IamfAudioElementData =
    std::unordered_map<std::string, std::span<const double>>;

struct IamfTemporalUnitData {
  // Mapping of parameter block IDs to serialized `ParameterBlockObuMetadata`
  // protocol buffer starting in this temporal unit.
  std::unordered_map<uint32_t, std::string> parameter_block_id_to_metadata;

  // All audio elements for this temporal unit.
  std::unordered_map<uint32_t, IamfAudioElementData> audio_element_id_to_data;
};

/*!\brief Sample rate for linear PCM audio substreams.*/
enum class PcmSampleRate {
  k16kHz = 16000,
  k32kHz = 32000,
  k44_1kHz = 44100,
  k48kHz = 48000,
  k96kHz = 96000,
};

/*!\brief Scene-based (Ambisonics) or channel-based full loudspeaker layout.*/
enum class AudioElementType {
  // Spherical Ambisonics (ACN/SN3D, (N + 1)^2 channels).
  kAmbisonicSphericalOrder1 = 1,
  kAmbisonicSphericalOrder2 = 2,
  kAmbisonicSphericalOrder3 = 3,
  kAmbisonicSphericalOrder4 = 4,

  // Horizontal Ambisonics (ACN/SN3D, 2N + 1 channels).
  kAmbisonicHorizontalOrder1 = 5,
  kAmbisonicHorizontalOrder2 = 6,
  kAmbisonicHorizontalOrder3 = 7,
  kAmbisonicHorizontalOrder4 = 8,

  // Standard Loudspeaker Layouts (LoudspeakerLayout).
  kLayoutMono = 9,
  kLayoutStereo = 10,
  kLayout5_1 = 11,
  kLayout5_1_2 = 12,
  kLayout5_1_4 = 13,
  kLayout7_1 = 14,
  kLayout7_1_2 = 15,
  kLayout7_1_4 = 16,
  kLayout3_1_2 = 17,
  kLayoutBinaural = 18,

  // Full Expanded Loudspeaker Layouts (ExpandedLoudspeakerLayout).
  kLayout9_1_6 = 19,
  kLayout10_2_9_3 = 20,
  kLayout7_1_5_4 = 21,
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_
