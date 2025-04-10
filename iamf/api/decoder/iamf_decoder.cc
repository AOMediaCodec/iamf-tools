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

#include "iamf/include/iamf_tools/iamf_decoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/api/conversion/channel_reorderer.h"
#include "iamf/api/conversion/mix_presentation_conversion.h"
#include "iamf/api/conversion/profile_conversion.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace api {

enum class DecoderStatus { kAcceptingData, kEndOfStream };

// Holds the internal state of the decoder to hide it and necessary includes
// from API users.
struct IamfDecoder::DecoderState {
  /*!\brief Constructor.
   *
   * \param read_bit_buffer Buffer to decode from.
   * \param requested_layout User-requested layout, the actual layout may be
   *        different depending on the mix presentations.
   * \param requested_profile_versions User-requested profile versions, the
   *        actual profile version may be different depending on the mix
   *        presentations.
   */
  DecoderState(std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer,
               const Layout& requested_layout,
               const absl::flat_hash_set<::iamf_tools::ProfileVersion>&
                   requested_profile_versions)
      : read_bit_buffer(std::move(read_bit_buffer)),
        layout(requested_layout),
        desired_profile_versions(requested_profile_versions) {}

  // Current status of the decoder.
  DecoderStatus status = DecoderStatus::kAcceptingData;

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

  // TODO(b/379122580):  Use the bit depth of the underlying content.
  // Defaulting to int32 for now.
  OutputSampleType output_sample_type = OutputSampleType::kInt32LittleEndian;

  // True iff the decoder was created via CreateFromDescriptors().
  bool created_from_descriptors = false;

  // Cache the profile versions that the user is interested in, we use them to
  // select an appropriate mix presentation.
  const absl::flat_hash_set<::iamf_tools::ProfileVersion>
      desired_profile_versions;

  // Once descriptors have been processed, they are stored here. This is useful
  // for Reset() purposes, in which we can recreate the ObuProcessor with the
  // original descriptors in order to ensure that the state of the processor is
  // clean.
  std::vector<uint8_t> descriptor_obus;

  ChannelReorderer::RearrangementScheme channel_rearrangement_scheme =
      ChannelReorderer::RearrangementScheme::kDefaultNoOp;
  // Created after DescriptorObus are processed and final Layout is known.
  std::optional<ChannelReorderer> channel_reorderer = std::nullopt;
};

namespace {
constexpr int kInitialBufferSize = 1024;

IamfStatus AbslToIamfStatus(const absl::Status& absl_status) {
  if (absl_status.ok()) {
    return IamfStatus::OkStatus();
  } else {
    return IamfStatus::ErrorStatus(
        absl_status.ToString(absl::StatusToStringMode::kDefault));
  }
}

ChannelReorderer::RearrangementScheme ChannelOrderingApiToInternalType(
    ChannelOrdering channel_ordering) {
  switch (channel_ordering) {
    case ChannelOrdering::kOrderingForAndroid:
      return ChannelReorderer::RearrangementScheme::kReorderForAndroid;
    case ChannelOrdering::kIamfOrdering:
    default:
      return ChannelReorderer::RearrangementScheme::kDefaultNoOp;
  }
}

// Creates an ObuProcessor; an ObuProcessor is only created once all descriptor
// OBUs have been processed. Contracted to only return a resource exhausted
// error if there is not enough data to process the descriptor OBUs.
absl::StatusOr<std::unique_ptr<ObuProcessor>> CreateObuProcessor(
    const absl::flat_hash_set<::iamf_tools::ProfileVersion>&
        desired_profile_versions,
    bool contains_all_descriptor_obus,
    StreamBasedReadBitBuffer* read_bit_buffer, Layout& in_out_layout,
    std::vector<uint8_t>& output_descriptor_obus) {
  // Happens only in the pure streaming case.
  const auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      desired_profile_versions, in_out_layout,
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
  const auto num_bytes_read = (read_bit_buffer->Tell() - start_position) / 8;

  // Seek back to the beginning of the data that was processed so that we can
  // read and store the binary IAMF descriptor OBUs.
  RETURN_IF_NOT_OK(read_bit_buffer->Seek(start_position));
  output_descriptor_obus.resize(num_bytes_read);
  RETURN_IF_NOT_OK(
      read_bit_buffer->ReadUint8Span(absl::MakeSpan(output_descriptor_obus)));
  RETURN_IF_NOT_OK(read_bit_buffer->Flush(num_bytes_read));
  return obu_processor;
}

IamfStatus ProcessAllTemporalUnits(
    StreamBasedReadBitBuffer* read_bit_buffer, ObuProcessor* obu_processor,
    bool created_from_descriptors,
    std::queue<std::vector<std::vector<int32_t>>>& rendered_pcm_samples,
    std::optional<ChannelReorderer> channel_reorderer) {
  LOG_FIRST_N(INFO, 10) << "Processing Temporal Units";
  bool continue_processing = true;
  const auto start_position_bits = read_bit_buffer->Tell();
  while (continue_processing) {
    std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
    // TODO(b/395889878): Add support for partial temporal units.
    absl::Status absl_status = obu_processor->ProcessTemporalUnit(
        created_from_descriptors, output_temporal_unit, continue_processing);
    if (!absl_status.ok()) {
      return AbslToIamfStatus(absl_status);
    }
    // We may have processed bytes but not a full temporal unit.
    if (output_temporal_unit.has_value()) {
      absl::Span<const std::vector<int32_t>>
          rendered_pcm_samples_for_temporal_unit;
      absl_status = obu_processor->RenderTemporalUnitAndMeasureLoudness(
          output_temporal_unit->output_timestamp,
          output_temporal_unit->output_audio_frames,
          output_temporal_unit->output_parameter_blocks,
          rendered_pcm_samples_for_temporal_unit);
      if (!absl_status.ok()) {
        return AbslToIamfStatus(absl_status);
      }
      auto temporal_unit =
          std::vector(rendered_pcm_samples_for_temporal_unit.begin(),
                      rendered_pcm_samples_for_temporal_unit.end());
      if (channel_reorderer.has_value()) {
        channel_reorderer->Reorder(temporal_unit);
      }
      rendered_pcm_samples.push(std::move(temporal_unit));
    }
  }
  // Empty the buffer of the data that was processed thus far.
  const auto num_bits_read = read_bit_buffer->Tell() - start_position_bits;
  absl::Status absl_status = read_bit_buffer->Flush(num_bits_read / 8);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }
  LOG_FIRST_N(INFO, 10) << "Rendered " << rendered_pcm_samples.size()
                        << " temporal units.";
  return IamfStatus::OkStatus();
}

size_t BytesPerSample(OutputSampleType sample_type) {
  switch (sample_type) {
    case OutputSampleType::kInt16LittleEndian:
      return 2;
    case OutputSampleType::kInt32LittleEndian:
      return 4;
    default:
      return 0;
  }
}

IamfStatus WriteFrameToSpan(const std::vector<std::vector<int32_t>>& frame,
                            OutputSampleType sample_type,
                            absl::Span<uint8_t> output_bytes,
                            size_t& bytes_written) {
  const size_t bytes_per_sample = BytesPerSample(sample_type);
  const size_t bits_per_sample = bytes_per_sample * 8;
  const size_t required_size =
      frame.size() * frame[0].size() * bytes_per_sample;
  if (output_bytes.size() < required_size) {
    return IamfStatus::ErrorStatus(
        "Invalid Argument: Span does not have enough space to write output "
        "bytes.");
  }
  const bool big_endian = false;
  size_t write_position = 0;
  uint8_t* data = output_bytes.data();
  for (int t = 0; t < frame.size(); t++) {
    for (int c = 0; c < frame[0].size(); ++c) {
      const uint32_t sample = static_cast<uint32_t>(frame[t][c]);
      absl::Status absl_status = WritePcmSample(
          sample, bits_per_sample, big_endian, data, write_position);
      if (!absl_status.ok()) {
        return AbslToIamfStatus(absl_status);
      }
    }
  }
  bytes_written = write_position;
  return IamfStatus::OkStatus();
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

IamfStatus IamfDecoder::Create(const Settings& settings,
                               std::unique_ptr<IamfDecoder>& output_decoder) {
  output_decoder = nullptr;

  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer =
      StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return IamfStatus::ErrorStatus(
        "Internal Error: Failed to create read bit buffer.");
  }

  // Cache the internal representation of the profile versions. Depending on
  // creation mode, we may not have all the descriptors yet.
  absl::flat_hash_set<::iamf_tools::ProfileVersion> desired_profile_versions;
  for (const auto& profile_version : settings.requested_profile_versions) {
    desired_profile_versions.insert(ApiToInternalType(profile_version));
  }

  std::unique_ptr<DecoderState> state = std::make_unique<DecoderState>(
      std::move(read_bit_buffer), ApiToInternalType(settings.requested_layout),
      desired_profile_versions);
  state->channel_rearrangement_scheme =
      ChannelOrderingApiToInternalType(settings.channel_ordering);
  output_decoder = absl::WrapUnique(new IamfDecoder(std::move(state)));
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::CreateFromDescriptors(
    const Settings& settings, const uint8_t* input_buffer,
    size_t input_buffer_size, std::unique_ptr<IamfDecoder>& output_decoder) {
  output_decoder = nullptr;
  absl::Span<const uint8_t> descriptor_obus(input_buffer, input_buffer_size);

  IamfStatus status = Create(settings, output_decoder);
  if (!status.ok()) {
    return status;
  }
  if (output_decoder == nullptr) {
    return IamfStatus::ErrorStatus("Internal Error: Unexpected null decoder");
  }
  absl::Status absl_status =
      output_decoder->state_->read_bit_buffer->PushBytes(descriptor_obus);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }

  absl::StatusOr<std::unique_ptr<ObuProcessor>> obu_processor =
      CreateObuProcessor(output_decoder->state_->desired_profile_versions,
                         /*contains_all_descriptor_obus=*/true,
                         output_decoder->state_->read_bit_buffer.get(),
                         output_decoder->state_->layout,
                         output_decoder->state_->descriptor_obus);
  if (!obu_processor.ok()) {
    return AbslToIamfStatus(obu_processor.status());
  }
  output_decoder->state_->obu_processor = *std::move(obu_processor);
  output_decoder->state_->created_from_descriptors = true;
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::Decode(const uint8_t* input_buffer,
                               size_t input_buffer_size) {
  if (state_->status == DecoderStatus::kEndOfStream) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: Decode() cannot be called after "
        "SignalEndOfStream() has been called.");
  }
  auto bitstream = absl::MakeConstSpan(input_buffer, input_buffer_size);
  absl::Status push_bytes_status =
      state_->read_bit_buffer->PushBytes(bitstream);
  if (!push_bytes_status.ok()) {
    return AbslToIamfStatus(push_bytes_status);
  }
  if (!IsDescriptorProcessingComplete()) {
    auto obu_processor = CreateObuProcessor(
        state_->desired_profile_versions,
        /*contains_all_descriptor_obus=*/false, state_->read_bit_buffer.get(),
        state_->layout, state_->descriptor_obus);
    if (obu_processor.ok()) {
      state_->obu_processor = *std::move(obu_processor);
      return IamfStatus::OkStatus();
    } else if (absl::IsResourceExhausted(obu_processor.status())) {
      // Don't have enough data to process the descriptor OBUs yet, but no
      // errors have occurred.
      return IamfStatus::OkStatus();
    } else {
      // Corrupted data or other errors.
      return AbslToIamfStatus(obu_processor.status());
    }
  }

  // At this stage, we know that we've processed all descriptor OBUs.
  if (std::holds_alternative<LoudspeakersSsConventionLayout>(
          state_->layout.specific_layout)) {
    auto sound_system =
        std::get<LoudspeakersSsConventionLayout>(state_->layout.specific_layout)
            .sound_system;
    state_->channel_reorderer = ChannelReorderer::Create(
        sound_system, state_->channel_rearrangement_scheme);
  }
  return ProcessAllTemporalUnits(
      state_->read_bit_buffer.get(), state_->obu_processor.get(),
      state_->created_from_descriptors, state_->rendered_pcm_samples,
      state_->channel_reorderer);
}

IamfStatus IamfDecoder::ConfigureMixPresentationId(
    MixPresentationId mix_presentation_id) {
  return IamfStatus::ErrorStatus(
      "Unimplemented: ConfigureMixPresentationId is not yet implemented.");
}

void IamfDecoder::ConfigureOutputSampleType(
    OutputSampleType output_sample_type) {
  state_->output_sample_type = output_sample_type;
}

IamfStatus IamfDecoder::GetOutputTemporalUnit(uint8_t* output_buffer,
                                              size_t output_buffer_size,
                                              size_t& bytes_written) {
  bytes_written = 0;
  if (state_->rendered_pcm_samples.empty()) {
    return IamfStatus::OkStatus();
  }
  OutputSampleType output_sample_type = GetOutputSampleType();
  IamfStatus status = WriteFrameToSpan(
      state_->rendered_pcm_samples.front(), output_sample_type,
      absl::MakeSpan(output_buffer, output_buffer_size), bytes_written);
  if (status.ok()) {
    state_->rendered_pcm_samples.pop();
  }
  return status;
}

bool IamfDecoder::IsTemporalUnitAvailable() const {
  return !state_->rendered_pcm_samples.empty();
}

bool IamfDecoder::IsDescriptorProcessingComplete() const {
  return state_->obu_processor != nullptr;
}

IamfStatus IamfDecoder::GetOutputLayout(OutputLayout& output_layout) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetOutputLayout() cannot be called before "
        "descriptor processing is complete.");
  }
  absl::StatusOr<OutputLayout> conversion = InternalToApiType(state_->layout);
  if (conversion.ok()) {
    output_layout = *conversion;
    return IamfStatus::OkStatus();
  }
  return AbslToIamfStatus(conversion.status());
}

IamfStatus IamfDecoder::GetNumberOfOutputChannels(
    int& output_num_channels) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetNumberOfOutputChannels() cannot be called "
        "before descriptor processing is complete.");
  }
  return AbslToIamfStatus(MixPresentationObu::GetNumChannelsFromLayout(
      state_->layout, output_num_channels));
}

IamfStatus IamfDecoder::GetMixPresentations(
    std::vector<MixPresentationMetadata>& output_mix_presentation_metadata)
    const {
  return IamfStatus::ErrorStatus(
      "Unimplemented: GetMixPresentations is not yet implemented.");
}

OutputSampleType IamfDecoder::GetOutputSampleType() const {
  return state_->output_sample_type;
}

IamfStatus IamfDecoder::GetSampleRate(uint32_t& output_sample_rate) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetSampleRate() cannot be called before "
        "descriptor processing is complete.");
  }
  absl::StatusOr<uint32_t> sample_rate =
      state_->obu_processor->GetOutputSampleRate();
  if (sample_rate.ok()) {
    output_sample_rate = *sample_rate;
    return IamfStatus::OkStatus();
  }
  return AbslToIamfStatus(sample_rate.status());
}

IamfStatus IamfDecoder::GetFrameSize(uint32_t& output_frame_size) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetFrameSize() cannot be called before "
        "descriptor processing is complete.");
  }

  absl::StatusOr<uint32_t> frame_size =
      state_->obu_processor->GetOutputFrameSize();
  if (frame_size.ok()) {
    output_frame_size = *frame_size;
    return IamfStatus::OkStatus();
  }
  return AbslToIamfStatus(frame_size.status());
}

IamfStatus IamfDecoder::Reset() {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: Reset() cannot be called before descriptor "
        "processing is complete.");
  }

  // Clear the rendered PCM samples.
  std::queue<std::vector<std::vector<int32_t>>> empty;
  std::swap(state_->rendered_pcm_samples, empty);

  // Set state.
  state_->status = DecoderStatus::kAcceptingData;
  // Create a new read bit buffer.
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer =
      StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return IamfStatus::ErrorStatus(
        "Internal Error: Failed to create read bit buffer.");
  }
  state_->read_bit_buffer = std::move(read_bit_buffer);

  // Create a new ObuProcessor with the original descriptor OBUs.
  absl::Status absl_status =
      state_->read_bit_buffer->PushBytes(state_->descriptor_obus);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }
  absl::StatusOr<std::unique_ptr<ObuProcessor>> obu_processor =
      CreateObuProcessor(state_->desired_profile_versions,
                         /*contains_all_descriptor_obus=*/true,
                         state_->read_bit_buffer.get(), state_->layout,
                         state_->descriptor_obus);
  if (!obu_processor.ok()) {
    return AbslToIamfStatus(obu_processor.status());
  }
  state_->obu_processor = *std::move(obu_processor);

  return IamfStatus::OkStatus();
}

void IamfDecoder::SignalEndOfDecoding() {
  state_->status = DecoderStatus::kEndOfStream;
}

IamfStatus IamfDecoder::Close() { return IamfStatus::OkStatus(); }

}  // namespace api
}  // namespace iamf_tools
