/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/audio_frame_generator.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/aac_encoder_decoder.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/encoder_base.h"
#include "iamf/cli/flac_encoder_decoder.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/lpcm_encoder.h"
#include "iamf/cli/opus_encoder_decoder.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/codec_config.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

namespace {

absl::Status InitializeEncoder(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config, int num_channels,
    std::unique_ptr<EncoderBase>& encoder) {
  switch (codec_config.codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      encoder = std::make_unique<LpcmEncoder>(codec_config, num_channels);
      break;
    case kCodecIdOpus:
      encoder = std::make_unique<OpusEncoder>(
          codec_config_metadata.decoder_config_opus().opus_encoder_metadata(),
          codec_config, num_channels);
      break;
    case kCodecIdAacLc:
      encoder = std::make_unique<AacEncoder>(
          codec_config_metadata.decoder_config_aac().aac_encoder_metadata(),
          codec_config, num_channels);
      break;
    case kCodecIdFlac:
      encoder = std::make_unique<FlacEncoder>(
          codec_config_metadata.decoder_config_flac().flac_encoder_metadata(),
          codec_config, num_channels);
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown codec_id= ", codec_config.codec_config_.codec_id));
  }
  RETURN_IF_NOT_OK(encoder->Initialize());
  return absl::OkStatus();
}

// Gets data relevant to encoding (Codec Config OBU and AudioElementWithData)
// and initializes encoders.
absl::Status GetEncodingDataAndInitializeEncoders(
    const absl::flat_hash_map<DecodedUleb128, iamf_tools_cli_proto::CodecConfig>
        codec_config_metadata,
    const AudioElementWithData& audio_element_with_data,
    absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>&
        substream_id_to_encoder) {
  for (const auto& [substream_id, labels] :
       audio_element_with_data.substream_id_to_labels) {
    const int num_channels = static_cast<int>(labels.size());
    const CodecConfigObu& codec_config_obu =
        *audio_element_with_data.codec_config;
    auto codec_config_metadata_iter =
        codec_config_metadata.find(codec_config_obu.codec_config_id_);
    if (codec_config_metadata_iter == codec_config_metadata.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find codec config metadata for codec_config_id= ",
          codec_config_obu.codec_config_id_));
    }

    RETURN_IF_NOT_OK(InitializeEncoder(codec_config_metadata_iter->second,
                                       codec_config_obu, num_channels,
                                       substream_id_to_encoder[substream_id]));
  }

  return absl::OkStatus();
}

absl::Status ValidateUserStartTrim(uint32_t user_samples_to_trim_at_start,
                                   uint32_t& encoder_required_samples_to_pad) {
  // Return an error. But obey the user when `-DNO_CHECK_ERROR` is set.
  if (user_samples_to_trim_at_start < encoder_required_samples_to_pad) {
    // Only pad up to what the user requests.
    const auto message =
        absl::StrCat("The encoder requires ", encoder_required_samples_to_pad,
                     " samples trimmed at the start but only ",
                     user_samples_to_trim_at_start, " were requested");
    encoder_required_samples_to_pad = user_samples_to_trim_at_start;
    return absl::InvalidArgumentError(message);
  }

  return absl::OkStatus();
}

absl::Status GetNumSamplesToPadAtEndAndValidate(
    uint32_t required_samples_to_pad, uint32_t& num_user_samples_left_to_trim,
    uint32_t& num_samples_to_pad_at_end) {
  num_samples_to_pad_at_end =
      std::min(required_samples_to_pad, num_user_samples_left_to_trim);
  if (num_user_samples_left_to_trim < required_samples_to_pad) {
    num_user_samples_left_to_trim = 0;
    // Obey the user's request by setting `num_user_samples_left_to_trim`. But
    // throw an error.
    return absl::InvalidArgumentError(
        absl::StrCat("User input requested ", num_user_samples_left_to_trim,
                     " trimmed samples. But required ", required_samples_to_pad,
                     " to pad a full frame"));
  }

  num_user_samples_left_to_trim -= num_samples_to_pad_at_end;
  return absl::OkStatus();
}

void PadSamples(const size_t num_samples_to_pad, const size_t num_channels,
                std::deque<std::vector<int32_t>>& samples) {
  samples.insert(samples.end(), num_samples_to_pad,
                 std::vector<int32_t>(num_channels, 0));
}

void MoveSamples(const size_t num_samples,
                 std::deque<std::vector<int32_t>>& source_samples,
                 std::vector<std::vector<int32_t>>& destination_samples) {
  CHECK_GE(source_samples.size(), num_samples);
  std::copy(source_samples.begin(), source_samples.begin() + num_samples,
            destination_samples.begin());
  source_samples.erase(source_samples.begin(),
                       source_samples.begin() + num_samples);
}

absl::Status InitializeSubstreamData(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>&
        substream_id_to_encoder,
    const uint32_t num_samples_to_trim_at_start,
    const uint32_t num_samples_to_trim_at_end,
    absl::flat_hash_map<uint32_t, uint32_t>& substream_id_to_samples_pad_end,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) {
  // Validate user start trim is correct; it depends on the encoder. Insert
  // the "virtual samples" at the start up to the amount required by the codec
  // and encoder into the `samples_obu` queue. Trimming of additional optional
  // samples will occur later to keep trimming logic in one place as much as
  // possible.
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    const auto encoder_iter = substream_id_to_encoder.find(substream_id);
    if (encoder_iter == substream_id_to_encoder.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find encoder for substream ID= ", substream_id));
    }

    uint32_t encoder_required_samples_to_delay =
        encoder_iter->second->GetNumberOfSamplesToDelayAtStart();
    RETURN_IF_NOT_OK(ValidateUserStartTrim(num_samples_to_trim_at_start,
                                           encoder_required_samples_to_delay));

    // Track number of samples requested and prevent trimming samples the user
    // did not request. Although an error will be thrown later as it creates an
    // invalid IAMF stream.
    // All substreams in the same `audio_frame_metadata` have the same user trim
    // applied.
    substream_id_to_samples_pad_end[substream_id] = num_samples_to_trim_at_end;

    // Initialize a `SubstreamData` with virtual samples for any delay
    // introduced by the encoder.
    auto& substream_data_for_id = substream_id_to_substream_data[substream_id];
    substream_data_for_id = {
        substream_id,
        /*samples_obu=*/{},
        /*samples_encode=*/{},
        /*output_gains_linear=*/{},
        /*num_samples_to_trim_at_end=*/0,
        /*num_samples_to_trim_at_start=*/encoder_required_samples_to_delay};

    PadSamples(encoder_required_samples_to_delay, labels.size(),
               substream_data_for_id.samples_obu);
  }

  return absl::OkStatus();
}

absl::Status DownMixSamples(const DecodedUleb128 audio_element_id,
                            const DemixingModule& demixing_module,
                            LabelSamplesMap& label_to_samples,
                            ParametersManager& parameters_manager,
                            absl::flat_hash_map<uint32_t, SubstreamData>&
                                substream_id_to_substream_data,
                            DownMixingParams& down_mixing_params) {
  RETURN_IF_NOT_OK(parameters_manager.GetDownMixingParameters(
      audio_element_id, down_mixing_params));
  LOG_FIRST_N(INFO, 10) << "Using alpha=" << down_mixing_params.alpha
                        << " beta=" << down_mixing_params.beta
                        << " gamma=" << down_mixing_params.gamma
                        << " delta=" << down_mixing_params.delta
                        << " w_idx_offset=" << down_mixing_params.w_idx_offset
                        << " w_idx_used=" << down_mixing_params.w_idx_used
                        << " w=" << down_mixing_params.w;

  // Down-mix OBU-aligned samples from input channels to substreams. May
  // generate intermediate channels (e.g. L3 on the way of down-mixing L7 to L2)
  // and expand `label_to_samples`.
  RETURN_IF_NOT_OK(demixing_module.DownMixSamplesToSubstreams(
      audio_element_id, down_mixing_params, label_to_samples,
      substream_id_to_substream_data));

  return absl::OkStatus();
}

// Gets the next frame of samples for all streams, either from "real" samples
// read from a file or from padding.
absl::Status GetNextFrameSubstreamData(
    const DecodedUleb128 audio_element_id,
    const DemixingModule& demixing_module, const size_t num_samples_per_frame,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    LabelSamplesMap& label_to_samples,
    absl::flat_hash_map<uint32_t, uint32_t>& substream_id_to_samples_pad_end,
    ParametersManager& parameters_manager,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data,
    DownMixingParams& down_mixing_params) {
  const bool no_sample_added =
      (label_to_samples.empty() || label_to_samples.begin()->second.empty());
  if (no_sample_added &&
      (substream_id_to_substream_data.empty() ||
       substream_id_to_substream_data.begin()->second.samples_obu.empty())) {
    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(DownMixSamples(
      audio_element_id, demixing_module, label_to_samples, parameters_manager,
      substream_id_to_substream_data, down_mixing_params));

  // Padding.
  for (const auto& [substream_id, unused_labels] : substream_id_to_labels) {
    auto& substream_data = substream_id_to_substream_data.at(substream_id);
    const int num_channels = substream_data.samples_obu.front().size();
    if (substream_data.samples_obu.size() < num_samples_per_frame) {
      uint32_t num_samples_to_trim_at_end;
      RETURN_IF_NOT_OK(GetNumSamplesToPadAtEndAndValidate(
          num_samples_per_frame - substream_data.samples_obu.size(),
          substream_id_to_samples_pad_end[substream_data.substream_id],
          num_samples_to_trim_at_end));

      PadSamples(num_samples_to_trim_at_end, num_channels,
                 substream_data.samples_obu);
      PadSamples(num_samples_to_trim_at_end, num_channels,
                 substream_data.samples_encode);

      substream_data.num_samples_to_trim_at_end = num_samples_to_trim_at_end;
    }

    if (no_sample_added &&
        substream_data.samples_encode.size() < num_samples_per_frame) {
      const uint32_t num_samples_to_pad =
          num_samples_per_frame - substream_data.samples_encode.size();

      // It's possible to be in this state for the final frame when there
      // are multiple padded frames at the start. Extra virtual samples
      // need to be added. These samples will be "left in" the decoder
      // after all OBUs are processed, but they should not count as being
      // trimmed.
      PadSamples(num_samples_to_pad, num_channels,
                 substream_data.samples_encode);
    }
  }

  return absl::OkStatus();
}

std::pair<uint32_t, uint32_t> UpdateNumSamplesToTrim(
    const uint32_t num_samples_to_encode,
    uint32_t& num_samples_left_to_trim_at_start,
    uint32_t& num_samples_left_to_trim_at_end) {
  const uint32_t num_samples_to_trim_at_end =
      std::min(num_samples_to_encode, num_samples_left_to_trim_at_end);
  num_samples_left_to_trim_at_end -= num_samples_to_trim_at_end;
  const uint32_t num_samples_to_trim_at_start =
      std::min(num_samples_to_encode, num_samples_left_to_trim_at_start);
  num_samples_left_to_trim_at_start -= num_samples_to_trim_at_start;

  return std::make_pair(num_samples_to_trim_at_start,
                        num_samples_to_trim_at_end);
}

absl::Status EncodeFramesForAudioElement(
    const DecodedUleb128 audio_element_id,
    const AudioElementWithData& audio_element_with_data,
    const DemixingModule& demixing_module, LabelSamplesMap& label_to_samples,
    absl::flat_hash_map<uint32_t, uint32_t>& substream_id_to_samples_pad_end,
    ParametersManager& parameters_manager,
    absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>&
        substream_id_to_encoder,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data,
    GlobalTimingModule& global_timing_module) {
  const CodecConfigObu& codec_config = *audio_element_with_data.codec_config;

  // Get some common information about this stream.
  const size_t num_samples_per_frame =
      static_cast<size_t>(codec_config.GetNumSamplesPerFrame());
  // TODO(b/310906409): Lossy codecs do not use PCM for internal
  //                    representation of data. We may need to measure loudness
  //                    at a different bit-depth than the input when AAC is
  //                    updated to support higher bit-depths.
  const int encoder_input_pcm_bit_depth =
      static_cast<int>(codec_config.GetBitDepthToMeasureLoudness());

  const uint32_t encoder_input_sample_rate = codec_config.GetInputSampleRate();

  const uint32_t decoder_output_sample_rate =
      codec_config.GetOutputSampleRate();
  if (encoder_input_sample_rate != decoder_output_sample_rate) {
    // TODO(b/280361524): Calculate `num_samples_to_trim_at_end` and timestamps
    //                    correctly when the input sample rate is different from
    //                    the output sample rate.
    return absl::InvalidArgumentError(absl::StrCat(
        "Input sample rate and output sample rate differ: (",
        encoder_input_sample_rate, " vs ", decoder_output_sample_rate, ")"));
  }

  DownMixingParams down_mixing_params;
  RETURN_IF_NOT_OK(GetNextFrameSubstreamData(
      audio_element_id, demixing_module, num_samples_per_frame,
      audio_element_with_data.substream_id_to_labels, label_to_samples,
      substream_id_to_samples_pad_end, parameters_manager,
      substream_id_to_substream_data, down_mixing_params));

  std::optional<int32_t> encoded_timestamp;
  for (const auto& [substream_id, unused_labels] :
       audio_element_with_data.substream_id_to_labels) {
    auto substream_data_iter =
        substream_id_to_substream_data.find(substream_id);
    if (substream_data_iter == substream_id_to_substream_data.end()) {
      continue;
    }

    auto& substream_data = substream_data_iter->second;
    if (substream_data.samples_obu.empty()) {
      substream_id_to_substream_data.erase(substream_data_iter);
      continue;
    }

    // Encode.
    auto& encoder = substream_id_to_encoder.at(substream_id);
    if (substream_data.samples_encode.size() < num_samples_per_frame &&
        !encoder->supports_partial_frames_) {
      // To support negative test-cases technically some encoders (such as
      // LPCM) can encode partial frames. For other encoders wait until there
      // is a whole frame of samples to encode.

      // All frames corresponding to the same audio element should be skipped.
      CHECK(!encoded_timestamp.has_value());

      LOG(INFO) << "Skipping partial frames; samples_obu.size()="
                << substream_data.samples_obu.size()
                << " samples_encode.size()= "
                << substream_data.samples_encode.size();
      continue;
    }

    // Pop samples from the queues and arrange in (time, channel) axes.
    // Take the minimum because some encoders support partial frames.
    const size_t num_samples_to_encode =
        std::min(static_cast<size_t>(num_samples_per_frame),
                 substream_data.samples_encode.size());
    std::vector<std::vector<int32_t>> samples_encode(num_samples_to_encode);
    std::vector<std::vector<int32_t>> samples_obu(num_samples_to_encode);

    MoveSamples(num_samples_to_encode, substream_data.samples_obu, samples_obu);
    MoveSamples(num_samples_to_encode, substream_data.samples_encode,
                samples_encode);
    const auto [num_samples_to_trim_at_start, num_samples_to_trim_at_end] =
        UpdateNumSamplesToTrim(num_samples_to_encode,
                               substream_data.num_samples_to_trim_at_start,
                               substream_data.num_samples_to_trim_at_end);

    // Both timestamps cover trimmed and regular samples.
    int32_t start_timestamp;
    int32_t end_timestamp;
    RETURN_IF_NOT_OK(global_timing_module.GetNextAudioFrameTimestamps(
        substream_data.substream_id, samples_obu.size(), start_timestamp,
        end_timestamp));

    if (encoded_timestamp.has_value()) {
      // All frames corresponding to the same audio element should have
      // the same start timestamp.
      CHECK_EQ(*encoded_timestamp, start_timestamp);
    }

    auto partial_audio_frame_with_data =
        absl::WrapUnique(new AudioFrameWithData{
            .obu = AudioFrameObu(
                {
                    .obu_trimming_status_flag =
                        (num_samples_to_trim_at_end != 0 ||
                         num_samples_to_trim_at_start != 0),
                    .num_samples_to_trim_at_end = num_samples_to_trim_at_end,
                    .num_samples_to_trim_at_start =
                        num_samples_to_trim_at_start,
                },
                substream_data.substream_id, {}),
            .start_timestamp = start_timestamp,
            .end_timestamp = end_timestamp,
            .raw_samples = samples_obu,
            .down_mixing_params = down_mixing_params,
            .audio_element_with_data = &audio_element_with_data});

    RETURN_IF_NOT_OK(
        encoder->EncodeAudioFrame(encoder_input_pcm_bit_depth, samples_encode,
                                  std::move(partial_audio_frame_with_data)));
    encoded_timestamp = start_timestamp;
  }

  if (encoded_timestamp.has_value()) {
    // An audio frame has been encoded, update the parameter manager to use
    // the next frame of parameters.
    RETURN_IF_NOT_OK(parameters_manager.UpdateDownMixingParameters(
        audio_element_id, *encoded_timestamp));
  }

  return absl::OkStatus();
}

absl::Status ValidateUserTrimming(
    const int64_t common_samples_to_trim_at_start,
    const int64_t common_samples_to_trim_at_end,
    const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata) {
  if (audio_frame_metadata.samples_to_trim_at_end() !=
          common_samples_to_trim_at_end ||
      audio_frame_metadata.samples_to_trim_at_start() !=
          common_samples_to_trim_at_start) {
    return absl::InvalidArgumentError(
        "Expected all substreams to have the same trimming "
        "information");
  }

  return absl::OkStatus();
}

/*\!brief Validates, tracks, and applies user trim to the input audio frame.
 *
 * The input arguments control whether this operates on fields that relate to
 * trimming from the start or from the end. Reference arguments must entirely
 * come from "end"-related fields or entirely from "start"-related fields.
 *
 * \param from_start `true` when log messages should refer to trimming from the
 *     start. `false` when log messages should refer to trimming from the end.
 * \param left_to_trim Reference to the number of samples left to trim for this
 *     substream
 * \param found_first_partial_frame Reference to the whether this substream
 *     has had at least one partial frame.
 * \param obu_samples_to_trim Reference to the `num_*_samples_to_trim` field in
 *     the OBU.
 * \param audio_frame Audio frame to process.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *     user input does not request enough frames trimmed from the end to make
 *     the OBU frame aligned. Or `absl::InvalidArgumentError()` when trimmed
 *     frames are found that are not consecutive with the start or end of the
 *     substream.
 */
absl::Status ApplyUserTrim(bool from_start, int64_t& left_to_trim,
                           bool& found_first_partial_frame,
                           uint32_t& obu_samples_to_trim,
                           AudioFrameWithData& audio_frame) {
  // Some samples may already be trimmed due to prior processing.
  const uint32_t already_trimmed_samples = obu_samples_to_trim;

  // Number of samples in a full frame.
  const uint32_t num_samples = audio_frame.raw_samples.size();

  // Trim as many samples as required. Up to the size of a full frame.
  const uint32_t num_to_trim = std::min((int64_t)num_samples, left_to_trim);

  // Validate that user input is consistent with automatically processed
  // trimming information.
  if (already_trimmed_samples > num_to_trim) {
    return absl::InvalidArgumentError(absl::StrCat(
        "More samples were trimmed from ",
        (from_start ? "the start" : "the end"),
        " than expected. Expected at least ", already_trimmed_samples,
        " samples to trim; got ", num_to_trim));
  }

  // Validate that trimmed frames must be consecutive from the start/end of the
  // substream.
  if (num_to_trim > 0 && found_first_partial_frame) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Found a padded frame, but a", (from_start ? "n earlier" : " latter"),
        " frame had some samples in it"));
  }

  if (num_to_trim != num_samples) {
    // Found the first partial frame. All subsequent frames cannot have padding.
    found_first_partial_frame = true;
  }

  // Apply the trim.
  obu_samples_to_trim = num_to_trim;
  left_to_trim -= num_to_trim;

  // Ensure the `obu_trimming_status_flag` is accurate.
  if (audio_frame.obu.header_.num_samples_to_trim_at_end != 0 ||
      audio_frame.obu.header_.num_samples_to_trim_at_start != 0) {
    audio_frame.obu.header_.obu_trimming_status_flag = 1;
  }

  // Obey the user when `NO_CHECK_ERROR` is set. But the spec never allows fully
  // trimmed frames from the end.
  if (!from_start && !found_first_partial_frame) {
    return absl::InvalidArgumentError(
        "The spec disallows trimming entire frames from the end");
  }

  return absl::OkStatus();
}

struct SubstreamState {
  int64_t samples_left_to_trim_end;
  int64_t samples_left_to_trim_start;
  bool found_first_partial_frame_from_end;
  bool found_first_partial_frame_from_start;
};

// Apply user trim to the input Audio Frames. User requested from the start will
// used to set the `num_samples_to_trim_at_start` field of consecutive OBUs from
// the start without modifying the underlying audio frame data. User requested
// trim from the end must be at least the amount that was needed to cover the
// padding in the final audio frame. Then the rest will be applied to
// consecutive OBUs from the end without modifying the underlying data.
absl::Status ValidateAndApplyUserTrimming(
    const absl::flat_hash_map<DecodedUleb128,
                              iamf_tools_cli_proto::AudioFrameObuMetadata>&
        audio_frame_metadata,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  // Track the state of each substream.
  absl::flat_hash_map<uint32_t, SubstreamState> substream_id_to_substream_state;

  // Simple and base profile require trimming information to be the same for all
  // substreams.
  if (audio_frame_metadata.empty()) {
    // Fine. There are no frames to validate.
    return absl::OkStatus();
  }
  const int64_t common_samples_to_trim_at_start = static_cast<int64_t>(
      audio_frame_metadata.begin()->second.samples_to_trim_at_start());
  const int64_t common_samples_to_trim_at_end = static_cast<int64_t>(
      audio_frame_metadata.begin()->second.samples_to_trim_at_end());

  for (const auto& [audio_element_id, metadata] : audio_frame_metadata) {
    // Validate the assumption that trimming is the same for all substreams.
    RETURN_IF_NOT_OK(ValidateUserTrimming(common_samples_to_trim_at_start,
                                          common_samples_to_trim_at_end,
                                          metadata));

    // Populate the list with all substream ID.
    const auto& audio_element_with_data = audio_elements.at(audio_element_id);
    for (const auto& [substream_id, labels] :
         audio_element_with_data.substream_id_to_labels) {
      substream_id_to_substream_state[substream_id] = {
          .samples_left_to_trim_end = common_samples_to_trim_at_end,
          .samples_left_to_trim_start = common_samples_to_trim_at_start,
          .found_first_partial_frame_from_end = false,
          .found_first_partial_frame_from_start = false,
      };
    }
  }

  // Trim all audio frames from the start.
  for (auto& audio_frame : audio_frames) {
    const uint32_t substream_id = audio_frame.obu.GetSubstreamId();
    const auto& substream_state_iter =
        substream_id_to_substream_state.find(substream_id);
    if (substream_state_iter == substream_id_to_substream_state.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unexpected substream_id= ", substream_id));
    }
    auto& substream_state = substream_state_iter->second;

    RETURN_IF_NOT_OK(ApplyUserTrim(
        /*from_start=*/true, substream_state.samples_left_to_trim_start,
        substream_state.found_first_partial_frame_from_start,
        audio_frame.obu.header_.num_samples_to_trim_at_start, audio_frame));
  }

  // Trim all audio frames from the end.
  for (std::list<AudioFrameWithData>::reverse_iterator audio_frame =
           audio_frames.rbegin();
       audio_frame != audio_frames.rend(); ++audio_frame) {
    auto& substream_state =
        substream_id_to_substream_state.at(audio_frame->obu.GetSubstreamId());

    RETURN_IF_NOT_OK(ApplyUserTrim(
        /*from_start=*/false, substream_state.samples_left_to_trim_end,
        substream_state.found_first_partial_frame_from_end,
        audio_frame->obu.header_.num_samples_to_trim_at_end, *audio_frame));
  }

  // Check that all audio frames had at least as many samples as requested to be
  // trimmed.
  for (const auto& [substream_id, substream_state] :
       substream_id_to_substream_state) {
    if (substream_state.samples_left_to_trim_end != 0 ||
        substream_state.samples_left_to_trim_start != 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("Too few samples to trim the requested amount in audio "
                       "substream [",
                       substream_id, "]"));
    }
  }

  return absl::OkStatus();
}

absl::Status DumpCodecSpecificHeader(const CodecConfigObu& codec_config_obu,
                                     WriteBitBuffer& wb,
                                     std::fstream& output_file) {
  // Flush the write buffer.
  wb.Reset();

  // Write a codec-specific header to make some files easier to debug.
  switch (codec_config_obu.codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdFlac:
      // Stamp on the "fLaC" header and `decoder_config_` to make a valid FLAC
      // stream.
      RETURN_IF_NOT_OK(wb.WriteUint8Vector({'f', 'L', 'a', 'C'}));
      RETURN_IF_NOT_OK(codec_config_obu.ValidateAndWriteDecoderConfig(wb));
      break;
    case kCodecIdAacLc:
      // Stamp on `decoder_config` to make a valid AAC stream.
      RETURN_IF_NOT_OK(codec_config_obu.ValidateAndWriteDecoderConfig(wb));
      break;
    default:
      return absl::OkStatus();
  }

  // Dump the write buffer to a file.
  RETURN_IF_NOT_OK(wb.FlushAndWriteToFile(output_file));

  return absl::OkStatus();
}

/*\!brief Opens up files based on the input map.
 *
 * Opens a `std::fstream` for each filename in `key_to_filename`. Stores the
 * resulting file stream in output argument `key_to_file_stream`. The keys
 * can be any unique ID.
 *
 * \param key_to_filename Map of key to the full path of the output file.
 * \param key_to_file_stream Map of key to opened `std::fstream`.
 * \return `absl::OkStatus()` on success. `absl::UnknownError()` if opening any
 *     file stream fails.
 */
absl::Status InitializeFileStreamMap(
    const absl::flat_hash_map<uint32_t, std::string>& key_to_filename,
    absl::flat_hash_map<uint32_t, std::fstream>& key_to_file_stream) {
  // Open a file based on the filename at each index of `key_to_filename`.
  for (const auto& [map_index, filename] : key_to_filename) {
    if (key_to_file_stream.find(map_index) != key_to_file_stream.end()) {
      continue;
    }
    key_to_file_stream[map_index].open(
        filename, std::fstream::out | std::fstream::binary);

    if (!key_to_file_stream[map_index]) {
      LOG(ERROR) << "Failed to open file: " << filename;
      return absl::UnknownError("");
    }
  }

  return absl::OkStatus();
}

// Dumps the Audio Frames without trimming applied to a file per substream. For
// LPCM streams the output is raw PCM. For Opus the output is the raw audio
// frames. For FLAC the output is the "fLaC", followed by the `decoder_config`
// and the raw FLAC frames. For AAC the output is the `decoder_config` followed
// by the raw AAC frames.
absl::Status DumpRawAudioFrames(
    const std::string& output_wav_directory, const std::string& file_prefix,
    const std::list<AudioFrameWithData>& audio_frames) {
  // A temporary resizable buffer to use when processing.
  const size_t kBufferSize = 1024;
  WriteBitBuffer wb(kBufferSize);

  absl::flat_hash_map<uint32_t, std::string> substream_ids_to_filename;
  absl::flat_hash_map<uint32_t, const CodecConfigObu*>
      substream_ids_to_codec_config;
  for (const auto& audio_frame : audio_frames) {
    const uint32_t substream_id = audio_frame.obu.GetSubstreamId();
    if (substream_ids_to_filename.find(substream_id) !=
        substream_ids_to_filename.end()) {
      // Skip if this substream ID was already processed.
      continue;
    }

    substream_ids_to_codec_config[substream_id] =
        audio_frame.audio_element_with_data->codec_config;

    // Pick a reasonable extension based on the type of data.
    std::string file_extension = ".bin";
    const uint32_t codec_id = audio_frame.audio_element_with_data->codec_config
                                  ->codec_config_.codec_id;
    if (codec_id == CodecConfig::kCodecIdFlac) {
      file_extension = ".flac";
    } else if (codec_id == CodecConfig::kCodecIdLpcm) {
      file_extension = ".pcm";
    } else if (codec_id == CodecConfig::kCodecIdAacLc) {
      file_extension = ".aac";
    } else if (codec_id == CodecConfig::kCodecIdOpus) {
      // The output is raw Opus frames, but usually Opus would be within ogg.
      // Although it would be possible to encapsulate this in ogg - for now just
      // label the data as binary.
      file_extension = ".bin";
    }

    const std::filesystem::path file_directory(output_wav_directory);
    const std::filesystem::path file_name(
        absl::StrCat(file_prefix, "_raw_audio_frame_",
                     audio_frame.obu.GetSubstreamId(), file_extension));
    // Write directly to special files (e.g. `/dev/null`). Otherwise append the
    // filename.
    substream_ids_to_filename[substream_id] =
        std::filesystem::is_character_file(file_directory)
            ? file_directory
            : file_directory / file_name;
  }

  // Map of substream ID to file stream.
  absl::flat_hash_map<uint32_t, std::fstream> substream_id_to_file_stream;
  // Open all files.
  RETURN_IF_NOT_OK(InitializeFileStreamMap(substream_ids_to_filename,
                                           substream_id_to_file_stream));

  // Apply a codec-specific header to assist in debugging.
  for (const auto& [substream_id, codec_config_obu] :
       substream_ids_to_codec_config) {
    RETURN_IF_NOT_OK(DumpCodecSpecificHeader(
        *codec_config_obu, wb, substream_id_to_file_stream[substream_id]));
  }

  // Write out all of the raw audio frames.
  for (const auto& audio_frame : audio_frames) {
    const uint32_t substream_id = audio_frame.obu.GetSubstreamId();

    RETURN_IF_NOT_OK(wb.WriteUint8Vector(audio_frame.obu.audio_frame_));
    RETURN_IF_NOT_OK(
        wb.FlushAndWriteToFile(substream_id_to_file_stream[substream_id]));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status AudioFrameGenerator::Initialize() {
  for (const auto& [audio_element_id, audio_frame_metadata] :
       audio_frame_metadata_) {
    // Find the Codec Config OBU for this mono or coupled stereo substream.
    const auto audio_elements_iter = audio_elements_.find(audio_element_id);
    if (audio_elements_iter == audio_elements_.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Audio Element with ID= ", audio_element_id, " not found"));
    }
    const AudioElementWithData& audio_element_with_data =
        audio_elements_iter->second;

    // Create an encoder for each substream.
    RETURN_IF_NOT_OK(GetEncodingDataAndInitializeEncoders(
        codec_config_metadata_, audio_element_with_data,
        substream_id_to_encoder_));

    // Intermediate data for each substream.
    RETURN_IF_NOT_OK(InitializeSubstreamData(
        audio_element_with_data.substream_id_to_labels,
        substream_id_to_encoder_,
        audio_frame_metadata.samples_to_trim_at_start(),
        audio_frame_metadata.samples_to_trim_at_end(),
        substream_id_to_samples_pad_end_, substream_id_to_substream_data_));

    // Validate that a `DemixingParamDefinition` is available if down-mixing
    // is needed.
    const std::list<Demixer>* down_mixers = nullptr;
    RETURN_IF_NOT_OK(
        demixing_module_.GetDownMixers(audio_element_id, down_mixers));
    if (!parameters_manager_.DemixingParamDefinitionAvailable(
            audio_element_id) &&
        !down_mixers->empty()) {
      return absl::InvalidArgumentError(
          "Must include `DemixingParamDefinition` in the Audio Element if "
          "down-mixers are required to produce audio substreams");
    }
  }

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::AddSamples(
    const DecodedUleb128 audio_element_id, const std::string& label,
    const std::vector<int32_t>& samples) {
  if (audio_frame_metadata_.find(audio_element_id) ==
      audio_frame_metadata_.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("No audio frame metadata found for Audio Element ID= ",
                     audio_element_id));
  }
  id_to_labeled_samples_[audio_element_id][label] = samples;

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::GenerateFrames() {
  for (auto& [audio_element_id, labeled_samples] : id_to_labeled_samples_) {
    const auto audio_element_iter = audio_elements_.find(audio_element_id);
    if (audio_element_iter == audio_elements_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("No Audio Element found for ID= ", audio_element_id));
    }

    RETURN_IF_NOT_OK(EncodeFramesForAudioElement(
        audio_element_id, audio_element_iter->second, demixing_module_,
        labeled_samples, substream_id_to_samples_pad_end_, parameters_manager_,
        substream_id_to_encoder_, substream_id_to_substream_data_,
        global_timing_module_));

    labeled_samples.clear();
  }

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::Finalize(
    std::list<AudioFrameWithData>& audio_frames) {
  for (auto& [unused_id, encoder] : substream_id_to_encoder_) {
    // Finalize all encoders.
    // TODO(b/306319126): When generating frames iteratively, need a way to
    //                    flush the frames without finalizing the encoder.
    RETURN_IF_NOT_OK(encoder->FinalizeAndFlush(audio_frames));
  }

  // Trim optional samples which make the input wav file shorter. This is done
  // after the fact to support multiple frames being trimmed at the end without
  // knowing the length of the stream beforehand.

  // TODO(b/306319126): When generating frames iteratively, trimming at the
  //                    start should happen for the first few frames, and
  //                    only trimming at the end should happen here in
  //                    `Finalize()`.
  RETURN_IF_NOT_OK(ValidateAndApplyUserTrimming(audio_frame_metadata_,
                                                audio_elements_, audio_frames));

  RETURN_IF_NOT_OK(DumpRawAudioFrames(output_wav_directory_, file_name_prefix_,
                                      audio_frames));

  // Examine the first, last, and any audio frames with `trimming_status_flag`
  // set.
  int i = 0;
  for (const auto& audio_frame_with_data : audio_frames) {
    if (i == 0 || i == audio_frames.size() - 1 ||
        audio_frame_with_data.obu.header_.obu_trimming_status_flag) {
      LOG(INFO) << "Audio Frame OBU[" << i << "]";

      audio_frame_with_data.obu.PrintObu();
      LOG(INFO) << "    audio frame.start_timestamp= "
                << audio_frame_with_data.start_timestamp;
      LOG(INFO) << "    audio frame.end_timestamp= "
                << audio_frame_with_data.end_timestamp;
    }
    i++;
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
