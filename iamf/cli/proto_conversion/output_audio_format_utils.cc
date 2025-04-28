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
#include "iamf/cli/proto_conversion/output_audio_format_utils.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "iamf/cli/proto/output_audio_format.pb.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {
typedef RenderingMixPresentationFinalizer::SampleProcessorFactory
    SampleProcessorFactory;
using iamf_tools_cli_proto::OutputAudioFormat;
}  // namespace

void ApplyOutputAudioFormatToSampleProcessorFactory(
    OutputAudioFormat output_audio_format,
    SampleProcessorFactory& sample_processor_factory) {
  // The bit-depth force when writing the wav file, or `std::nullopt` to use the
  // bit-depth of the input audio.
  uint8_t override_bit_depth = 0;
  switch (output_audio_format) {
    using enum iamf_tools_cli_proto::OutputAudioFormat;
    case OUTPUT_FORMAT_INVALID:
      LOG(WARNING) << "Invalid output audio format. Disabling output audio.";
      [[fallthrough]];
    case OUTPUT_FORMAT_NONE:
      // Disable wav writing entirely.
      sample_processor_factory =
          RenderingMixPresentationFinalizer::ProduceNoSampleProcessors;
      return;
    case OUTPUT_FORMAT_WAV_BIT_DEPTH_AUTOMATIC:
      // Preserve the factory. Later the bit-depth will be inferred from the
      // input audio.
      return;
    // Modes which force the bit-depth of the output wav file.
    case OUTPUT_FORMAT_WAV_BIT_DEPTH_SIXTEEN:
      override_bit_depth = 16;
      break;
    case OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR:
      override_bit_depth = 24;
      break;
    case OUTPUT_FORMAT_WAV_BIT_DEPTH_THIRTY_TWO:
      override_bit_depth = 32;
      break;
  }

  // Override the bit-depth to match the requested format.
  sample_processor_factory =
      [original_factory = std::move(sample_processor_factory),
       override_bit_depth](DecodedUleb128 mix_presentation_id,
                           int sub_mix_index, int layout_index,
                           const Layout& layout, int num_channels,
                           int sample_rate, int /*bit_depth*/,
                           size_t max_input_samples_per_frame)
      -> std::unique_ptr<SampleProcessorBase> {
    return original_factory(mix_presentation_id, sub_mix_index, layout_index,
                            layout, num_channels, sample_rate,
                            override_bit_depth, max_input_samples_per_frame);
  };
}

}  // namespace iamf_tools
