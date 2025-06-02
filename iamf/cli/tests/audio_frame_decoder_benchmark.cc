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

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/aac_encoder.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/codec/flac_encoder.h"
#include "iamf/cli/codec/lpcm_encoder.h"
#include "iamf/cli/codec/opus_encoder.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr DecodedUleb128 kCodecConfigId = 57;
constexpr uint32_t kSampleRate = 48000;
constexpr uint8_t kSampleSize = 16;
constexpr int kOneChannel = 1;
constexpr bool kValidateCodecDelay = true;
constexpr DecodedUleb128 kAudioElementId = 9;
constexpr DecodedUleb128 kSubstreamId = 11;

static std::unique_ptr<AacEncoder> CreateAacEncoder(
    const CodecConfigObu& codec_config) {
  iamf_tools_cli_proto::AacEncoderMetadata aac_encoder_metadata;
  aac_encoder_metadata.set_bitrate_mode(0);
  aac_encoder_metadata.set_enable_afterburner(true);
  aac_encoder_metadata.set_signaling_mode(2);
  auto encoder = std::make_unique<AacEncoder>(aac_encoder_metadata,
                                              codec_config, kOneChannel);
  return encoder;
}

static std::unique_ptr<FlacEncoder> CreateFlacEncoder(
    const CodecConfigObu& codec_config) {
  // Encoder.
  iamf_tools_cli_proto::FlacEncoderMetadata flac_encoder_metadata;
  flac_encoder_metadata.set_compression_level(0);
  auto encoder = std::make_unique<FlacEncoder>(flac_encoder_metadata,
                                               codec_config, kOneChannel);
  return encoder;
}

static std::unique_ptr<OpusEncoder> CreateOpusEncoder(
    const CodecConfigObu& codec_config) {
  // Encoder.
  iamf_tools_cli_proto::OpusEncoderMetadata opus_encoder_metadata;
  opus_encoder_metadata.set_target_bitrate_per_channel(48000);
  opus_encoder_metadata.set_application(
      iamf_tools_cli_proto::APPLICATION_AUDIO);
  auto encoder = std::make_unique<OpusEncoder>(
      opus_encoder_metadata, codec_config, kOneChannel, kSubstreamId);
  return encoder;
}

static AudioFrameWithData PrepareEncodedAudioFrame(
    const uint32_t num_samples_per_frame,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    CodecConfig::CodecId codec_id_type) {
  std::unique_ptr<EncoderBase> encoder;
  if (codec_id_type == CodecConfig::kCodecIdAacLc) {
    AddAacCodecConfig(kCodecConfigId, num_samples_per_frame, kSampleRate,
                      codec_config_obus);
    encoder = CreateAacEncoder(codec_config_obus.at(kCodecConfigId));
  } else if (codec_id_type == CodecConfig::kCodecIdFlac) {
    AddFlacCodecConfig(kCodecConfigId, num_samples_per_frame, kSampleRate,
                       kSampleSize, codec_config_obus);
    encoder = CreateFlacEncoder(codec_config_obus.at(kCodecConfigId));
  } else if (codec_id_type == CodecConfig::kCodecIdLpcm) {
    AddLpcmCodecConfig(kCodecConfigId, num_samples_per_frame, kSampleSize,
                       kSampleRate, codec_config_obus);
    encoder = std::make_unique<LpcmEncoder>(
        codec_config_obus.at(kCodecConfigId), kOneChannel);
  } else if (codec_id_type == CodecConfig::kCodecIdOpus) {
    AddOpusCodecConfig(kCodecConfigId, num_samples_per_frame, kSampleRate,
                       codec_config_obus);
    encoder = CreateOpusEncoder(codec_config_obus.at(kCodecConfigId));
  }
  CHECK_NE(encoder, nullptr);
  CHECK_OK(encoder->Initialize(kValidateCodecDelay));

  std::vector<uint8_t> encoded_audio_frame_payload = {};
  auto partial_audio_frame_with_data = absl::WrapUnique(new AudioFrameWithData{
      .obu = AudioFrameObu(
          {
              .obu_trimming_status_flag = false,
              .num_samples_to_trim_at_end = 0,
              .num_samples_to_trim_at_start = 0,
          },
          kSubstreamId, absl::MakeConstSpan(encoded_audio_frame_payload)),
      .start_timestamp = 0,
      .end_timestamp = num_samples_per_frame,
  });

  // Encode a frame of one channel with `num_samples_per_frame` samples.
  std::vector<std::vector<int32_t>> pcm_samples(kOneChannel);
  pcm_samples[0].resize(num_samples_per_frame, 0);
  CHECK_OK(encoder->EncodeAudioFrame(kSampleSize, pcm_samples,
                                     std::move(partial_audio_frame_with_data)));
  std::list<AudioFrameWithData> output_audio_frames;
  CHECK_OK(encoder->Finalize());
  CHECK_OK(encoder->Pop(output_audio_frames));

  return output_audio_frames.back();
}

static void InitAudioFrameDecoder(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    AudioFrameDecoder& decoder) {
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {kSubstreamId}, codec_config_obus,
      audio_elements);
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    CHECK_OK(decoder.InitDecodersForSubstreams(
        audio_element_with_data.substream_id_to_labels,
        *audio_element_with_data.codec_config));
  }
}

static void BM_DecodeForCodecId(const CodecConfig::CodecId codec_id_type,
                                benchmark::State& state) {
  // Prepare the input, which is an encoded audio frame.
  const uint32_t num_samples_per_frame = state.range(0);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  AudioFrameWithData audio_frame = PrepareEncodedAudioFrame(
      num_samples_per_frame, codec_config_obus, codec_id_type);

  // Prepare the audio frame decoder.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AudioFrameDecoder decoder;
  InitAudioFrameDecoder(codec_config_obus, audio_elements, decoder);

  // Measure the calls to `AudioFrameDecoder::Decode()`, which decodes a frame.
  for (auto _ : state) {
    CHECK_OK(decoder.Decode(audio_frame));
  }
}

static void BM_DecodeAac(benchmark::State& state) {
  BM_DecodeForCodecId(CodecConfig::kCodecIdAacLc, state);
}

static void BM_DecodeFlac(benchmark::State& state) {
  BM_DecodeForCodecId(CodecConfig::kCodecIdFlac, state);
}

static void BM_DecodeLpcm(benchmark::State& state) {
  BM_DecodeForCodecId(CodecConfig::kCodecIdLpcm, state);
}

static void BM_DecodeOpus(benchmark::State& state) {
  BM_DecodeForCodecId(CodecConfig::kCodecIdOpus, state);
}

// Benchmark with various numbers of samples per frame.
BENCHMARK(BM_DecodeFlac)->Args({480})->Args({960})->Args({1920});
BENCHMARK(BM_DecodeLpcm)->Args({480})->Args({960})->Args({1920});
BENCHMARK(BM_DecodeOpus)->Args({480})->Args({960})->Args({1920});

// AAC-LC only supports a frame size of 1024.
BENCHMARK(BM_DecodeAac)->Args({1024});

}  // namespace
}  // namespace iamf_tools
