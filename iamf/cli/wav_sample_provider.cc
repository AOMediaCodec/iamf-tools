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

#include "iamf/cli/wav_sample_provider.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/ia.h"

namespace iamf_tools {

absl::Status WavSampleProvider::Initialize(
    const std::string& input_wav_directory,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  for (const auto& [audio_element_id, audio_frame_metadata] :
       audio_frame_metadata_) {
    if (audio_frame_metadata.channel_ids_size() !=
        audio_frame_metadata.channel_labels_size()) {
      return absl::InvalidArgumentError(
          absl::StrCat("#channel IDs and #channel labels differ: (",
                       audio_frame_metadata.channel_ids_size(), " vs ",
                       audio_frame_metadata.channel_labels_size(), ")"));
    }

    const auto audio_element_iter = audio_elements.find(audio_element_id);
    if (audio_element_iter == audio_elements.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("No Audio Element found for ID= ", audio_element_id));
    }
    const auto& codec_config = *audio_element_iter->second.codec_config;
    const auto& wav_filename = std::filesystem::path(input_wav_directory) /
                               audio_frame_metadata.wav_filename();

    auto [wav_reader_iter, emplaced] = wav_readers_.emplace(
        audio_element_id,
        std::make_unique<WavReader>(
            wav_filename.c_str(),
            static_cast<size_t>(codec_config.GetNumSamplesPerFrame())));
    const auto& wav_reader = *wav_reader_iter->second;

    const int encoder_input_pcm_bit_depth =
        static_cast<int>(codec_config.GetBitDepthToMeasureLoudness());
    if (wav_reader.bit_depth() > encoder_input_pcm_bit_depth) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Refusing to lower bit-depth of WAV (", wav_filename.string(),
          ") with bit_depth= ", wav_reader.bit_depth(),
          " to bit_depth=", encoder_input_pcm_bit_depth));
    }

    const uint32_t encoder_input_sample_rate =
        codec_config.GetInputSampleRate();
    if (wav_reader.sample_rate_hz() != encoder_input_sample_rate) {
      // TODO(b/277899855): Support resampling the input wav file to match the
      //                    input sample rate.
      return absl::InvalidArgumentError(absl::StrCat(
          "Sample rate read from ", wav_filename.string(),
          " inconsistent with the user metadata: (",
          wav_reader.sample_rate_hz(), " vs ", encoder_input_sample_rate, ")"));
    }

    const uint32_t decoder_output_sample_rate =
        codec_config.GetOutputSampleRate();
    if (encoder_input_sample_rate != decoder_output_sample_rate) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Input and output sample rates differ: (", encoder_input_sample_rate,
          " vs ", decoder_output_sample_rate, ")"));
    }
  }

  return absl::OkStatus();
}

absl::Status WavSampleProvider::ReadFrames(
    const DecodedUleb128 audio_element_id, LabelSamplesMap& labeled_samples) {
  auto wav_reader_iter = wav_readers_.find(audio_element_id);
  if (wav_reader_iter == wav_readers_.end()) {
    LOG(ERROR) << "No WAV reader found for Audio Element ID= "
               << audio_element_id;
    return absl::InvalidArgumentError("");
  }
  auto& wav_reader = *wav_reader_iter->second;
  const size_t samples_read = wav_reader.ReadFrame();
  LOG_FIRST_N(INFO, 1) << samples_read << " samples read";

  // Note if the WAV reader is found for the Audio Element ID, then it's
  // guaranteed to have a corresponding audio frame metadata (otherwise the
  // `Initialize()` would have failed).
  const auto& audio_frame_metadata = audio_frame_metadata_.at(audio_element_id);
  const size_t num_time_ticks = samples_read / wav_reader.num_channels();
  const auto& channel_ids = audio_frame_metadata.channel_ids();
  const auto& channel_labels = audio_frame_metadata.channel_labels();
  labeled_samples.clear();
  for (int c = 0; c < channel_labels.size(); ++c) {
    auto& samples = labeled_samples[channel_labels[c]];
    samples.resize(num_time_ticks);
    for (int t = 0; t < num_time_ticks; ++t) {
      samples[t] = wav_reader.buffers_[t][channel_ids[c]];
    }
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
