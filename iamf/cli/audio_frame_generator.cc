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
#include "absl/synchronization/mutex.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/aac_encoder.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/codec/flac_encoder.h"
#include "iamf/cli/codec/lpcm_encoder.h"
#include "iamf/cli/codec/opus_encoder.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/common/macros.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

absl::Status InitializeEncoder(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config, int num_channels,
    std::unique_ptr<EncoderBase>& encoder) {
  switch (codec_config.GetCodecConfig().codec_id) {
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
          "Unknown codec_id= ", codec_config.GetCodecConfig().codec_id));
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
        codec_config_metadata.find(codec_config_obu.GetCodecConfigId());
    if (codec_config_metadata_iter == codec_config_metadata.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find codec config metadata for codec_config_id= ",
          codec_config_obu.GetCodecConfigId()));
    }

    RETURN_IF_NOT_OK(InitializeEncoder(codec_config_metadata_iter->second,
                                       codec_config_obu, num_channels,
                                       substream_id_to_encoder[substream_id]));
  }

  return absl::OkStatus();
}

// Validates that the user requested number of samples to trim at start is
// enough to cover the delay that the encoder needs.
absl::Status ValidateUserStartTrim(
    uint32_t user_samples_to_trim_at_start,
    uint32_t& encoder_required_samples_to_delay) {
  // Return an error. But obey the user when `-DNO_CHECK_ERROR` is set.
  if (user_samples_to_trim_at_start < encoder_required_samples_to_delay) {
    // Only pad up to what the user requests.
    const auto message =
        absl::StrCat("The encoder requires ", encoder_required_samples_to_delay,
                     " samples trimmed at the start but only ",
                     user_samples_to_trim_at_start, " were requested");
    encoder_required_samples_to_delay = user_samples_to_trim_at_start;
    return absl::InvalidArgumentError(message);
  }

  return absl::OkStatus();
}

absl::Status GetNumSamplesToPadAtEndAndValidate(
    const uint32_t required_samples_to_pad_at_end,
    const uint32_t user_samples_to_trim_at_end,
    uint32_t& num_samples_to_pad_at_end) {
  num_samples_to_pad_at_end =
      std::min(required_samples_to_pad_at_end, user_samples_to_trim_at_end);
  if (user_samples_to_trim_at_end < required_samples_to_pad_at_end) {
    // Obey the user's request by setting `user_samples_to_trim_at_end`. But
    // throw an error.
    return absl::InvalidArgumentError(
        absl::StrCat("User input requested ", user_samples_to_trim_at_end,
                     " trimmed samples. But ", required_samples_to_pad_at_end,
                     " samples are required to pad a full frame"));
  }

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
    const uint32_t user_samples_to_trim_at_start,
    const uint32_t user_samples_to_trim_at_end,
    absl::flat_hash_map<uint32_t, uint32_t>&
        substream_id_to_user_samples_trim_end,
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
    RETURN_IF_NOT_OK(ValidateUserStartTrim(user_samples_to_trim_at_start,
                                           encoder_required_samples_to_delay));

    // Track number of samples requested and prevent trimming samples the user
    // did not request. Although an error will be thrown later as it creates an
    // invalid IAMF stream.
    // All substreams in the same Audio Element have the same user trim
    // applied.
    substream_id_to_user_samples_trim_end[substream_id] =
        user_samples_to_trim_at_end;

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

// An audio element may contain many channels, denoted by their labels;
// this function returns whether all labels have their (same amount of)
// samples ready.
bool SamplesReadyForAudioElement(
    const LabelSamplesMap& label_to_samples,
    const ::google::protobuf::RepeatedPtrField<std::string>& channel_labels) {
  size_t common_num_samples = 0;
  for (const auto& label : channel_labels) {
    const auto label_to_samples_iter = label_to_samples.find(label);
    if (label_to_samples_iter == label_to_samples.end()) {
      return false;
    }

    const auto num_samples = label_to_samples_iter->second.size();
    if (common_num_samples == 0 && num_samples != 0) {
      common_num_samples = num_samples;
      continue;
    }

    if (num_samples != common_num_samples) {
      return false;
    }
  }

  return true;
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
    const absl::flat_hash_map<uint32_t, uint32_t>&
        substream_id_to_user_samples_trim_end,
    LabelSamplesMap& label_to_samples, ParametersManager& parameters_manager,
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
      uint32_t num_samples_to_pad_at_end;
      RETURN_IF_NOT_OK(GetNumSamplesToPadAtEndAndValidate(
          num_samples_per_frame - substream_data.samples_obu.size(),
          substream_id_to_user_samples_trim_end.at(substream_id),
          num_samples_to_pad_at_end));

      PadSamples(num_samples_to_pad_at_end, num_channels,
                 substream_data.samples_obu);
      PadSamples(num_samples_to_pad_at_end, num_channels,
                 substream_data.samples_encode);

      // Record the number of padded samples to be trimmed later.
      substream_data.num_samples_to_trim_at_end = num_samples_to_pad_at_end;
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

// Take as many samples as possible out of the total number of samples to trim,
// up to the size of a full frame.
std::pair<uint32_t, uint32_t> GetNumSamplesToTrimForFrame(
    const uint32_t num_samples_in_frame, uint32_t& num_samples_to_trim_at_start,
    uint32_t& num_samples_to_trim_at_end) {
  const uint32_t frame_samples_to_trim_at_end =
      std::min(num_samples_in_frame, num_samples_to_trim_at_end);
  num_samples_to_trim_at_end -= frame_samples_to_trim_at_end;
  const uint32_t frame_samples_to_trim_at_start =
      std::min(num_samples_in_frame, num_samples_to_trim_at_start);
  num_samples_to_trim_at_start -= frame_samples_to_trim_at_start;

  return std::make_pair(frame_samples_to_trim_at_start,
                        frame_samples_to_trim_at_end);
}

absl::Status EncodeFramesForAudioElement(
    const DecodedUleb128 audio_element_id,
    const AudioElementWithData& audio_element_with_data,
    const DemixingModule& demixing_module, LabelSamplesMap& label_to_samples,
    const absl::flat_hash_map<uint32_t, uint32_t>&
        substream_id_to_user_samples_trim_end,
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
      audio_element_with_data.substream_id_to_labels,
      substream_id_to_user_samples_trim_end, label_to_samples,
      parameters_manager, substream_id_to_substream_data, down_mixing_params));

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

      // All frames corresponding to the same Audio Element should be skipped.
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
    const auto [frame_samples_to_trim_at_start, frame_samples_to_trim_at_end] =
        GetNumSamplesToTrimForFrame(num_samples_to_encode,
                                    substream_data.num_samples_to_trim_at_start,
                                    substream_data.num_samples_to_trim_at_end);

    // Both timestamps cover trimmed and regular samples.
    int32_t start_timestamp;
    int32_t end_timestamp;
    RETURN_IF_NOT_OK(global_timing_module.GetNextAudioFrameTimestamps(
        substream_id, samples_obu.size(), start_timestamp, end_timestamp));

    if (encoded_timestamp.has_value()) {
      // All frames corresponding to the same Audio Element should have
      // the same start timestamp.
      CHECK_EQ(*encoded_timestamp, start_timestamp);
    }

    auto partial_audio_frame_with_data =
        absl::WrapUnique(new AudioFrameWithData{
            .obu = AudioFrameObu(
                {
                    .obu_trimming_status_flag =
                        (frame_samples_to_trim_at_end != 0 ||
                         frame_samples_to_trim_at_start != 0),
                    .num_samples_to_trim_at_end = frame_samples_to_trim_at_end,
                    .num_samples_to_trim_at_start =
                        frame_samples_to_trim_at_start,
                },
                substream_id, {}),
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
    RETURN_IF_NOT_OK(parameters_manager.UpdateDemixingState(
        audio_element_id, *encoded_timestamp));
  }

  return absl::OkStatus();
}

// Validates that all substreams share the same trimming information.
absl::Status ValidateSubstreamsShareTrimming(
    const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata,
    int64_t& common_samples_to_trim_at_start,
    int64_t& common_samples_to_trim_at_end) {
  // Set to the first seen values if uninitialized.
  if (common_samples_to_trim_at_start == -1) {
    common_samples_to_trim_at_start =
        static_cast<int64_t>(audio_frame_metadata.samples_to_trim_at_start());
  }
  if (common_samples_to_trim_at_end == -1) {
    common_samples_to_trim_at_end =
        static_cast<int64_t>(audio_frame_metadata.samples_to_trim_at_end());
  }

  if (audio_frame_metadata.samples_to_trim_at_end() !=
          common_samples_to_trim_at_end ||
      audio_frame_metadata.samples_to_trim_at_start() !=
          common_samples_to_trim_at_start) {
    return absl::InvalidArgumentError(
        "Expected all substreams to have the same trimming information");
  }

  return absl::OkStatus();
}

// Applies additional user trimming to one audio frame.
absl::Status ApplyUserTrimForFrame(const bool from_start,
                                   const uint32_t num_samples_in_frame,
                                   int64_t& user_trim_left,
                                   uint32_t& num_samples_trimmed_in_obu,
                                   bool& obu_trimming_status_flag) {
  // Trim as many samples as the user requested. Up to the size of a full frame.
  const uint32_t frame_samples_to_trim =
      std::min(static_cast<uint32_t>(num_samples_in_frame),
               static_cast<uint32_t>(user_trim_left));

  const std::string start_or_end_string = (from_start ? "start" : "end");

  // Some samples may already be trimmed due to prior processing, validate
  // that the user requested enough samples to accommodate them.
  if (num_samples_trimmed_in_obu > frame_samples_to_trim) {
    return absl::InvalidArgumentError(
        absl::StrCat("More samples were trimmed from the ", start_or_end_string,
                     "than expected: (", num_samples_trimmed_in_obu, " vs ",
                     frame_samples_to_trim, ")"));
  }

  // Apply the trim for this frame.
  num_samples_trimmed_in_obu = frame_samples_to_trim;
  user_trim_left -= frame_samples_to_trim;

  // Ensure the `obu_trimming_status_flag` is accurate.
  if (num_samples_trimmed_in_obu != 0) {
    obu_trimming_status_flag = true;
  }

  if (frame_samples_to_trim == num_samples_in_frame) {
    // Obey the user when `NO_CHECK_ERROR` is set. But the spec never allows
    // fully trimmed frames from the end.
    if (!from_start) {
      return absl::InvalidArgumentError(
          "The spec disallows trimming entire frames from the end");
    }
  }

  return absl::OkStatus();
}

// Apply user requested from the end to the input Audio Frames. The requested
// trim must be at least the amount that was needed to cover the
// padding in the final audio frame. Then the rest will be applied to
// consecutive OBUs from the end without modifying the underlying data.
absl::Status ValidateAndApplyUserTrimming(
    const uint32_t substream_id,  // For logging purposes only.
    const bool last_frame, AudioFrameGenerator::TrimmingState& trimming_state,
    AudioFrameWithData& audio_frame) {
  RETURN_IF_NOT_OK(ApplyUserTrimForFrame(
      /*from_start=*/true, audio_frame.raw_samples.size(),
      trimming_state.user_samples_left_to_trim_at_start,
      audio_frame.obu.header_.num_samples_to_trim_at_start,
      audio_frame.obu.header_.obu_trimming_status_flag));

  if (last_frame) {
    RETURN_IF_NOT_OK(ApplyUserTrimForFrame(
        /*from_start=*/false, audio_frame.raw_samples.size(),
        trimming_state.user_samples_left_to_trim_at_end,
        audio_frame.obu.header_.num_samples_to_trim_at_end,
        audio_frame.obu.header_.obu_trimming_status_flag));
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<uint32_t> AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config) {
  std::unique_ptr<EncoderBase> encoder;
  RETURN_IF_NOT_OK(InitializeEncoder(codec_config_metadata, codec_config,
                                     /*num_channels=*/1, encoder));
  if (encoder == nullptr) {
    return absl::InvalidArgumentError("Failed to initialize encoder");
  }
  return encoder->GetNumberOfSamplesToDelayAtStart();
}
absl::Status AudioFrameGenerator::Initialize() {
  int64_t common_samples_to_trim_at_start = -1;
  int64_t common_samples_to_trim_at_end = -1;
  for (const auto& [audio_element_id, audio_frame_metadata] :
       audio_frame_metadata_) {
    absl::MutexLock lock(&mutex_);

    // Find the Codec Config OBU for this mono or coupled stereo substream.
    const auto audio_elements_iter = audio_elements_.find(audio_element_id);
    if (audio_elements_iter == audio_elements_.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Audio Element with ID= ", audio_element_id, " not found"));
    }

    // Create an encoder for each substream.
    const AudioElementWithData& audio_element_with_data =
        audio_elements_iter->second;
    RETURN_IF_NOT_OK(GetEncodingDataAndInitializeEncoders(
        codec_config_metadata_, audio_element_with_data,
        substream_id_to_encoder_));

    // Intermediate data for all substreams belonging to an Audio Element.
    RETURN_IF_NOT_OK(
        InitializeSubstreamData(audio_element_with_data.substream_id_to_labels,
                                substream_id_to_encoder_,
                                audio_frame_metadata.samples_to_trim_at_start(),
                                audio_frame_metadata.samples_to_trim_at_end(),
                                substream_id_to_user_samples_trim_end_,
                                substream_id_to_substream_data_));

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

    // Validate the assumption that trimming is the same for all substreams.
    RETURN_IF_NOT_OK(ValidateSubstreamsShareTrimming(
        audio_frame_metadata, common_samples_to_trim_at_start,
        common_samples_to_trim_at_end));

    // Populate the map of trimming states with all substream ID.
    for (const auto& [substream_id, labels] :
         audio_element_with_data.substream_id_to_labels) {
      substream_id_to_trimming_state_[substream_id] = {
          .user_samples_left_to_trim_at_end = common_samples_to_trim_at_end,
          .user_samples_left_to_trim_at_start = common_samples_to_trim_at_start,
      };
    }
  }

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::AddSamples(
    const DecodedUleb128 audio_element_id, const std::string& label,
    const std::vector<int32_t>& samples) {
  const auto& audio_frame_metadata_iter =
      audio_frame_metadata_.find(audio_element_id);
  if (audio_frame_metadata_iter == audio_frame_metadata_.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("No audio frame metadata found for Audio Element ID= ",
                     audio_element_id));
  }
  auto& labeled_samples = id_to_labeled_samples_[audio_element_id];
  labeled_samples[label] = samples;

  const auto audio_element_iter = audio_elements_.find(audio_element_id);
  if (audio_element_iter == audio_elements_.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("No Audio Element found for ID= ", audio_element_id));
  }
  const auto& audio_element_with_data = audio_element_iter->second;

  if (SamplesReadyForAudioElement(
          labeled_samples,
          audio_frame_metadata_iter->second.channel_labels())) {
    absl::MutexLock lock(&mutex_);
    RETURN_IF_NOT_OK(EncodeFramesForAudioElement(
        audio_element_id, audio_element_with_data, demixing_module_,
        labeled_samples, substream_id_to_user_samples_trim_end_,
        parameters_manager_, substream_id_to_encoder_,
        substream_id_to_substream_data_, global_timing_module_));

    labeled_samples.clear();
  }

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::OutputFrames(
    std::list<AudioFrameWithData>& audio_frames) {
  absl::MutexLock lock(&mutex_);

  for (auto substream_id_to_encoder_iter = substream_id_to_encoder_.begin();
       substream_id_to_encoder_iter != substream_id_to_encoder_.end();) {
    auto& [substream_id, encoder] = *substream_id_to_encoder_iter;
    if (encoder->FramesAvailable()) {
      RETURN_IF_NOT_OK(encoder->Pop(audio_frames));
      RETURN_IF_NOT_OK(ValidateAndApplyUserTrimming(
          substream_id, encoder->Finished(),
          substream_id_to_trimming_state_.at(substream_id),
          audio_frames.back()));
    }

    // Remove finished encoder or advance the iterator.
    if (encoder->Finished()) {
      substream_id_to_encoder_.erase(substream_id_to_encoder_iter++);
    } else {
      ++substream_id_to_encoder_iter;
    }
  }

  return absl::OkStatus();
}

absl::Status AudioFrameGenerator::Finalize() {
  absl::MutexLock lock(&mutex_);
  for (auto& [unused_substream_id, encoder] : substream_id_to_encoder_) {
    // Signal all encoders that there are no more samples to come.
    RETURN_IF_NOT_OK(encoder->Finalize());
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
