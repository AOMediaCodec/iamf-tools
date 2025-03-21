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

#include "iamf/api/decoder/iamf_decoder.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/api/conversion/mix_presentation_conversion.h"
#include "iamf/api/types.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace api {

enum class Status { kAcceptingData, kFlushCalled };

// Holds the internal state of the decoder to hide it and necessary includes
// from API users.
struct IamfDecoder::DecoderState {
  DecoderState(std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer,
               const Layout& initial_requested_layout)
      : read_bit_buffer(std::move(read_bit_buffer)),
        layout(initial_requested_layout) {}

  // Current status of the decoder.
  Status status = Status::kAcceptingData;

  // Used to process descriptor OBUs and temporal units. Is only created after
  // the descriptor OBUs have been parsed.
  std::unique_ptr<ObuProcessor> obu_processor;

  // Buffer that is filled with data from Decode().
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer;

  // Rendered PCM samples. Each element in the queue corresponds to a
  // temporal unit. A temporal unit will never be partially filled, so the
  // number of elements in the outer vector is equal to the number of decoded
  // temporal units currently available.
  std::queue<std::vector<std::vector<int32_t>>> rendered_pcm_samples;

  // The layout used for the rendered output audio.
  // Initially set to the requested Layout but updated by ObuProcessor.
  Layout layout;
};

namespace {
constexpr int kInitialBufferSize = 1024;

// Creates an ObuProcessor; an ObuProcessor is only created once all descriptor
// OBUs have been processed. Contracted to only return a resource exhausted
// error if there is not enough data to process the descriptor OBUs.
absl::StatusOr<std::unique_ptr<ObuProcessor>> CreateObuProcessor(
    bool contains_all_descriptor_obus, absl::Span<const uint8_t> bitstream,
    StreamBasedReadBitBuffer* read_bit_buffer, Layout& in_out_layout) {
  // Happens only in the pure streaming case.
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      in_out_layout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/contains_all_descriptor_obus, read_bit_buffer,
      in_out_layout, insufficient_data);
  if (obu_processor == nullptr) {
    // `insufficient_data` is true iff everything so far is valid but more data
    // is needed.
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
    std::queue<std::vector<std::vector<int32_t>>>& rendered_pcm_samples) {
  LOG(INFO) << "Processing Temporal Units";
  int32_t num_bits_read = 0;
  bool continue_processing = true;
  while (continue_processing) {
    auto start_position_for_temporal_unit = read_bit_buffer->Tell();
    std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
    // TODO(b/395889878): Add support for partial temporal units.
    RETURN_IF_NOT_OK(obu_processor->ProcessTemporalUnit(
        /*eos_is_end_of_sequence=*/false, output_temporal_unit,
        continue_processing));
    if (!output_temporal_unit.has_value()) {
      break;
    }

    // Trivial IA Sequences may have empty temporal units. Do not try to
    // render empty temporal unit.
    if (output_temporal_unit.has_value()) {
      absl::Span<const std::vector<int32_t>>
          rendered_pcm_samples_for_temporal_unit;
      RETURN_IF_NOT_OK(obu_processor->RenderTemporalUnitAndMeasureLoudness(
          output_temporal_unit->output_timestamp,
          output_temporal_unit->output_audio_frames,
          output_temporal_unit->output_parameter_blocks,
          rendered_pcm_samples_for_temporal_unit));
      rendered_pcm_samples.push(
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

IamfDecoder::IamfDecoder(std::unique_ptr<DecoderState> state)
    : state_(std::move(state)) {}

// While these are all `= default`, they must be here in the source file because
// the unique_ptr of the partial class, DecoderState, prevents them from being
// inline.
IamfDecoder::~IamfDecoder() = default;
IamfDecoder::IamfDecoder(IamfDecoder&&) = default;
IamfDecoder& IamfDecoder::operator=(IamfDecoder&&) = default;

absl::StatusOr<IamfDecoder> IamfDecoder::Create(
    const OutputLayout& requested_layout) {
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer =
      StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return absl::InternalError("Failed to create read bit buffer.");
  }
  std::unique_ptr<DecoderState> state = std::make_unique<DecoderState>(
      std::move(read_bit_buffer), ApiToInternalType(requested_layout));
  return IamfDecoder(std::move(state));
}

absl::StatusOr<IamfDecoder> IamfDecoder::CreateFromDescriptors(
    const OutputLayout& requested_layout,
    absl::Span<const uint8_t> descriptor_obus) {
  absl::StatusOr<IamfDecoder> decoder = Create(requested_layout);
  if (!decoder.ok()) {
    return decoder.status();
  }
  RETURN_IF_NOT_OK(
      decoder->state_->read_bit_buffer->PushBytes(descriptor_obus));
  absl::StatusOr<std::unique_ptr<ObuProcessor>> obu_processor =
      CreateObuProcessor(/*contains_all_descriptor_obus=*/true, descriptor_obus,
                         decoder->state_->read_bit_buffer.get(),
                         decoder->state_->layout);
  if (!obu_processor.ok()) {
    return obu_processor.status();
  }
  decoder->state_->obu_processor = *std::move(obu_processor);
  return decoder;
}

absl::Status IamfDecoder::Decode(absl::Span<const uint8_t> bitstream) {
  if (state_->status == Status::kFlushCalled) {
    return absl::FailedPreconditionError(
        "Decode() cannot be called after Flush() has been called.");
  }
  RETURN_IF_NOT_OK(state_->read_bit_buffer->PushBytes(bitstream));
  if (!IsDescriptorProcessingComplete()) {
    auto obu_processor = CreateObuProcessor(
        /*contains_all_descriptor_obus=*/false, bitstream,
        state_->read_bit_buffer.get(), state_->layout);
    if (obu_processor.ok()) {
      state_->obu_processor = *std::move(obu_processor);
      return absl::OkStatus();
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
  RETURN_IF_NOT_OK(ProcessAllTemporalUnits(state_->read_bit_buffer.get(),
                                           state_->obu_processor.get(),
                                           state_->rendered_pcm_samples));
  return absl::OkStatus();
}

absl::Status IamfDecoder::ConfigureMixPresentationId(
    MixPresentationId mix_presentation_id) {
  return absl::UnimplementedError(
      "ConfigureMixPresentationId is not yet implemented.");
}

absl::Status IamfDecoder::ConfigureBitDepth(OutputFileBitDepth bit_depth) {
  return absl::UnimplementedError("ConfigureBitDepth is not yet implemented.");
}

absl::Status IamfDecoder::GetOutputTemporalUnit(
    std::vector<std::vector<int32_t>>& output_decoded_temporal_unit) {
  if (state_->rendered_pcm_samples.empty()) {
    output_decoded_temporal_unit.clear();
    return absl::OkStatus();
  }
  output_decoded_temporal_unit = state_->rendered_pcm_samples.front();
  state_->rendered_pcm_samples.pop();
  return absl::OkStatus();
}

bool IamfDecoder::IsTemporalUnitAvailable() const {
  return !state_->rendered_pcm_samples.empty();
}

bool IamfDecoder::IsDescriptorProcessingComplete() const {
  return state_->obu_processor != nullptr;
}

absl::StatusOr<OutputLayout> IamfDecoder::GetOutputLayout() const {
  if (!IsDescriptorProcessingComplete()) {
    return absl::FailedPreconditionError(
        "GetOutputLayout() cannot be called before descriptor processing is "
        "complete.");
  }
  return InternalToApiType(state_->layout);
}

absl::StatusOr<int> IamfDecoder::GetNumberOfOutputChannels() const {
  if (!IsDescriptorProcessingComplete()) {
    return absl::FailedPreconditionError(
        "GetNumberOfOutputChannels() cannot be called before descriptor "
        "processing is complete.");
  }
  int num_channels;
  RETURN_IF_NOT_OK(MixPresentationObu::GetNumChannelsFromLayout(state_->layout,
                                                                num_channels));
  return num_channels;
}

absl::Status IamfDecoder::GetMixPresentations(
    std::vector<MixPresentationMetadata>& output_mix_presentation_metadata)
    const {
  return absl::UnimplementedError(
      "GetMixPresentations is not yet implemented.");
}

absl::StatusOr<uint32_t> IamfDecoder::GetSampleRate() const {
  if (!IsDescriptorProcessingComplete()) {
    return absl::FailedPreconditionError(
        "GetSampleRate() cannot be called before descriptor processing is "
        "complete.");
  }

  return state_->obu_processor->GetOutputSampleRate();
}

absl::StatusOr<uint32_t> IamfDecoder::GetFrameSize() const {
  if (!IsDescriptorProcessingComplete()) {
    return absl::FailedPreconditionError(
        "GetFrameSize() cannot be called before descriptor processing is "
        "complete.");
  }

  return state_->obu_processor->GetOutputFrameSize();
}

absl::Status IamfDecoder::Flush(
    std::vector<std::vector<int32_t>>& output_decoded_temporal_unit,
    bool& output_is_done) {
  state_->status = Status::kFlushCalled;
  RETURN_IF_NOT_OK(GetOutputTemporalUnit(output_decoded_temporal_unit));
  output_is_done = state_->rendered_pcm_samples.empty();
  return absl::OkStatus();
}

absl::Status IamfDecoder::Close() { return absl::OkStatus(); }

}  // namespace api
}  // namespace iamf_tools
