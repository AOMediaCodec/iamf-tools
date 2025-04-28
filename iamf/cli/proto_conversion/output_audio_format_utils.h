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

#ifndef CLI_PROTO_CONVERSION_OUTPUT_AUDIO_FORMAT_UTILS_H_
#define CLI_PROTO_CONVERSION_OUTPUT_AUDIO_FORMAT_UTILS_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/output_audio_format.pb.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"

namespace iamf_tools {

/*!\brief Modifies a factory function for creating sample processors.
 *
 * \param output_audio_format Requested format of the output audio.
 * \param sample_processor_factory Factory function to modify in place.
 */
void ApplyOutputAudioFormatToSampleProcessorFactory(
    iamf_tools_cli_proto::OutputAudioFormat output_audio_format,
    RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory);

/*!\brief Converts a bit-depth to an `OutputAudioFormat`.
 *
 * \param bit_depth Override bit-depth.
 * \return `OutputAudioFormat` corresponding to the bit-depth, or an error if
 *         the bit-depth is not supported.
 */
absl::StatusOr<iamf_tools_cli_proto::OutputAudioFormat>
GetOutputAudioFormatFromBitDepth(uint8_t bit_depth);

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_OUTPUT_AUDIO_FORMAT_UTILS_H_
