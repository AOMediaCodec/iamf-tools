/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/api/iamf_decoder.h"

#include <cstdint>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

namespace {
constexpr int kInitialBufferSize = 1024;
constexpr Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};

// Creates an ObuProcessor; an ObuProcessor is only created once all descriptor
// OBUs have been processed. Contracted to only return a resource exhausted
// error if there is not enough data to process the descriptor OBUs.
absl::StatusOr<std::unique_ptr<ObuProcessor>> CreateObuProcessor(
    bool contains_all_descriptor_obus, absl::Span<const uint8_t> bitstream,
    StreamBasedReadBitBuffer* read_bit_buffer) {
  // Happens only in the pure streaming case.
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  // TODO(b/394376153): Update once we support other layouts.
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/contains_all_descriptor_obus, read_bit_buffer,
      insufficient_data);
  if (obu_processor == nullptr) {
    if (insufficient_data && !contains_all_descriptor_obus) {
      return absl::ResourceExhaustedError(
          "Have not received enough data yet to process descriptor "
          "OBUs. Please call Decode() again with more data.");
    }
    return absl::InvalidArgumentError("Failed to create OBU processor.");
  }
  auto num_bits_read = read_bit_buffer->Tell() - start_position;
  RETURN_IF_NOT_OK(read_bit_buffer->Flush(num_bits_read / 8));
  return obu_processor;
}

absl::Status ProcessAllTemporalUnits(
    StreamBasedReadBitBuffer* read_bit_buffer, ObuProcessor* obu_processor,
    std::vector<std::vector<std::vector<int32_t>>>& rendered_pcm_samples) {
  LOG(INFO) << "Processing Temporal Units";
  int32_t num_bits_read = 0;
  bool continue_processing = true;
  while (continue_processing) {
    auto start_position_for_temporal_unit = read_bit_buffer->Tell();
    std::list<AudioFrameWithData> audio_frames_for_temporal_unit;
    std::list<ParameterBlockWithData> parameter_blocks_for_temporal_unit;
    std::optional<int32_t> timestamp_for_temporal_unit;
    // TODO(b/395889878): Add support for partial temporal units.
    RETURN_IF_NOT_OK(obu_processor->ProcessTemporalUnit(
        audio_frames_for_temporal_unit, parameter_blocks_for_temporal_unit,
        timestamp_for_temporal_unit, continue_processing));

    // Trivial IA Sequences may have empty temporal units. Do not try to
    // render empty temporal unit.
    if (timestamp_for_temporal_unit.has_value()) {
      absl::Span<const std::vector<int32_t>>
          rendered_pcm_samples_for_temporal_unit;
      RETURN_IF_NOT_OK(obu_processor->RenderTemporalUnitAndMeasureLoudness(
          *timestamp_for_temporal_unit, audio_frames_for_temporal_unit,
          parameter_blocks_for_temporal_unit,
          rendered_pcm_samples_for_temporal_unit));
      rendered_pcm_samples.push_back(
          std::vector(rendered_pcm_samples_for_temporal_unit.begin(),
                      rendered_pcm_samples_for_temporal_unit.end()));
    }
    num_bits_read +=
        (read_bit_buffer->Tell() - start_position_for_temporal_unit);
  }
  // Empty the buffer of the data that was processed thus far.
  RETURN_IF_NOT_OK(read_bit_buffer->Flush(num_bits_read / 8));
  LOG(INFO) << "Rendered " << rendered_pcm_samples.size()
            << " temporal units. Please call GetOutputTemporalUnit() to get "
               "the rendered PCM samples.";
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<IamfDecoder> IamfDecoder::Create() {
  auto read_bit_buffer = StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return absl::InternalError("Failed to create read bit buffer.");
  }
  return IamfDecoder(std::move(read_bit_buffer));
}

absl::StatusOr<IamfDecoder> IamfDecoder::CreateFromDescriptors(
    absl::Span<const uint8_t> descriptor_obus) {
  auto decoder = Create();
  if (!decoder.ok()) {
    return decoder.status();
  }
  RETURN_IF_NOT_OK(decoder->read_bit_buffer_->PushBytes(std::vector<uint8_t>(
      std::begin(descriptor_obus), std::end(descriptor_obus))));
  auto obu_processor =
      CreateObuProcessor(/*contains_all_descriptor_obus=*/true, descriptor_obus,
                         decoder->read_bit_buffer_.get());
  if (!obu_processor.ok()) {
    return obu_processor.status();
  }
  decoder->obu_processor_ = *std::move(obu_processor);
  return decoder;
}

absl::Status IamfDecoder::Decode(absl::Span<const uint8_t> bitstream) {
  RETURN_IF_NOT_OK(read_bit_buffer_->PushBytes(
      std::vector<uint8_t>(std::begin(bitstream), std::end(bitstream))));
  if (!IsDescriptorProcessingComplete()) {
    auto obu_processor = CreateObuProcessor(
        /*contains_all_descriptor_obus=*/false, bitstream,
        read_bit_buffer_.get());
    if (obu_processor.ok()) {
      obu_processor_ = *std::move(obu_processor);
    } else if (absl::IsResourceExhausted(obu_processor.status())) {
      // Don't have enough data to process the descriptor OBUs yet, but no
      // errors have occurred.
      return absl::OkStatus();
    } else {
      // Corrupted data or other errors.
      return obu_processor.status();
    }
  }

  // At this stage, we know that we've processed all descriptor OBUs.
  RETURN_IF_NOT_OK(ProcessAllTemporalUnits(
      read_bit_buffer_.get(), obu_processor_.get(), rendered_pcm_samples_));
  return absl::OkStatus();
}

bool IamfDecoder::IsDescriptorProcessingComplete() {
  return obu_processor_ != nullptr;
}
}  // namespace iamf_tools
