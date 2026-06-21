/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/probe.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr DecodedUleb128 kAudioElementId = 2;
constexpr DecodedUleb128 kSubstreamId = 18;
constexpr DecodedUleb128 kMixPresentationId = 3;
constexpr DecodedUleb128 kParameterId = 999;
constexpr DecodedUleb128 kParameterRate = 48000;

// Appends a trailing temporal unit so the descriptor parser has a clear stop.
void AppendAudioFrame(DecodedUleb128 substream_id, std::vector<uint8_t>* data) {
  const AudioFrameObu audio_frame(ObuHeader(), substream_id,
                                  /*audio_frame=*/{1, 2, 3, 4});
  const auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  data->insert(data->end(), temporal_unit.begin(), temporal_unit.end());
}

std::vector<uint8_t> SerializeMinimalIaSequence(size_t* descriptor_size) {
  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                {kSubstreamId}, codec_configs,
                                                audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithAudioElementIds(kMixPresentationId,
                                           {kAudioElementId}, kParameterId,
                                           kParameterRate, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  *descriptor_size = data.size();
  AppendAudioFrame(kSubstreamId, &data);
  return data;
}

TEST(Probe, EmptyInputReportsResourceExhausted) {
  // Empty input is indistinguishable from a stream whose bytes have not
  // arrived yet, so it reports "need more data", not "invalid".
  const std::vector<uint8_t> empty;
  EXPECT_THAT(Probe(absl::MakeConstSpan(empty)).status(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST(Probe, TruncatedDescriptorsReportResourceExhausted) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto truncated =
      absl::MakeConstSpan(data).subspan(0, descriptor_size / 2);
  EXPECT_THAT(Probe(truncated).status(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST(Probe, IncrementalProbingSucceedsOnceDescriptorsAreComplete) {
  // The kResourceExhausted/kInvalidArgument split exists so callers can
  // grow the buffer and retry; exercise that pattern.
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  for (size_t len = 0; len < descriptor_size; len += 16) {
    const auto report = Probe(absl::MakeConstSpan(data).subspan(0, len));
    if (report.ok()) {
      // Parsing may legitimately succeed before `descriptor_size` once all
      // descriptor OBUs are visible; nothing more to assert.
      return;
    }
    ASSERT_THAT(report.status(),
                StatusIs(absl::StatusCode::kResourceExhausted));
  }
  EXPECT_THAT(Probe(absl::MakeConstSpan(data)), IsOk());
}

TEST(Probe, NonIamfInputReportsInvalidArgument) {
  // A codec config OBU arriving before any IA Sequence Header is
  // structurally invalid, not short.
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  const auto header_bytes = SerializeObusExpectOk({&sequence_header});
  // Strip the leading IA Sequence Header so a codec config OBU comes first.
  const auto headerless =
      absl::MakeConstSpan(data).subspan(header_bytes.size());
  EXPECT_THAT(Probe(headerless).status(),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Probe, ReportsProfilesAndDescriptors) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  EXPECT_EQ(report->primary_profile, "simple");
  EXPECT_EQ(report->primary_profile_raw,
            static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile));
  EXPECT_EQ(report->additional_profile, "base");
  EXPECT_EQ(report->additional_profile_raw,
            static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile));
  ASSERT_EQ(report->codec_configs.size(), 1);
  EXPECT_EQ(report->codec_configs.front().id, kCodecConfigId);
  EXPECT_EQ(report->codec_configs.front().codec_id, "Opus");
  // 'Opus' as a big-endian fourcc.
  EXPECT_EQ(report->codec_configs.front().codec_id_raw, 0x4f707573u);
  ASSERT_EQ(report->audio_elements.size(), 1);
  EXPECT_EQ(report->audio_elements.front().id, kAudioElementId);
  EXPECT_EQ(report->audio_elements.front().type, "scene_based");
  EXPECT_EQ(report->audio_elements.front().type_raw,
            1);  // AUDIO_ELEMENT_SCENE_BASED.
  ASSERT_EQ(report->mix_presentations.size(), 1);
  EXPECT_EQ(report->mix_presentations.front().id, kMixPresentationId);
  EXPECT_EQ(report->descriptor_bytes_consumed, descriptor_size);
}

TEST(Probe, PopulatesOpusDecoderConfig) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->codec_configs.size(), 1);
  const auto& cc = report->codec_configs.front();
  ASSERT_TRUE(cc.opus.has_value());
  EXPECT_FALSE(cc.lpcm.has_value());
  EXPECT_EQ(cc.opus->output_channel_count, 2);
  EXPECT_EQ(cc.opus->input_sample_rate, 48000);
  // The raw Q7.8 gain and its decoded float travel together.
  EXPECT_FLOAT_EQ(cc.opus->output_gain,
                  static_cast<float>(cc.opus->output_gain_q7_8) / 256.0f);
}

TEST(Probe, PopulatesAmbisonicsMonoDetails) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->audio_elements.size(), 1);
  const auto& ae = report->audio_elements.front();
  EXPECT_EQ(ae.type, "scene_based");
  ASSERT_TRUE(ae.ambisonics.has_value());
  EXPECT_EQ(ae.ambisonics->mode, "mono");
  ASSERT_TRUE(ae.ambisonics->mono.has_value());
  EXPECT_EQ(ae.substream_ids, std::vector<uint32_t>{kSubstreamId});
  // `num_channels` mirrors the ambisonics output channel count.
  ASSERT_TRUE(ae.num_channels.has_value());
  EXPECT_EQ(*ae.num_channels, ae.ambisonics->mono->output_channel_count);
  EXPECT_FALSE(ae.channel_layout_raw.has_value());
}

TEST(Probe, PopulatesScalableChannelLayerDetails) {
  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {kSubstreamId},
      codec_configs, audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(kSubstreamId, &data);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->codec_configs.size(), 1);
  ASSERT_TRUE(report->codec_configs.front().lpcm.has_value());
  EXPECT_EQ(report->codec_configs.front().lpcm->sample_rate, 48000);

  ASSERT_EQ(report->audio_elements.size(), 1);
  const auto& ae = report->audio_elements.front();
  EXPECT_EQ(ae.type, "channel_based");
  ASSERT_FALSE(ae.scalable_layers.empty());
  EXPECT_EQ(ae.scalable_layers.front().loudspeaker_layout, "Stereo");
  // The raw coded value (LOUDSPEAKER_LAYOUT_STEREO) rides alongside.
  EXPECT_EQ(ae.scalable_layers.front().loudspeaker_layout_raw, 1);
  // Top-layer convenience fields: stable layout id and channel count.
  ASSERT_TRUE(ae.channel_layout_raw.has_value());
  EXPECT_EQ(*ae.channel_layout_raw, 1);
  EXPECT_FALSE(ae.expanded_channel_layout_raw.has_value());
  ASSERT_TRUE(ae.num_channels.has_value());
  EXPECT_EQ(*ae.num_channels, 2u);  // One coupled substream = stereo.

  ASSERT_EQ(report->mix_presentations.size(), 1);
  const auto& mp = report->mix_presentations.front();
  EXPECT_EQ(mp.num_sub_mixes, 1);
  ASSERT_EQ(mp.sub_mixes.size(), 1);
  ASSERT_EQ(mp.sub_mixes.front().layouts.size(), 1);
  EXPECT_EQ(mp.sub_mixes.front()
                .layouts.front()
                .loudness_layout.sound_system.value_or(""),
            "Stereo");
  ASSERT_EQ(mp.sub_mixes.front().audio_elements.size(), 1);
  EXPECT_EQ(mp.sub_mixes.front().audio_elements.front().audio_element_id,
            kAudioElementId);
}

TEST(Probe, ComputesNumChannelsAcrossScalableLayers) {
  // Stereo base layer (one coupled substream = 2 channels) plus a 5.1
  // enhancement layer (one coupled + two mono substreams = 4 channels):
  // the top layout's channel count is the sum across layers, 6.
  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000, codec_configs);
  const std::vector<ChannelAudioLayerConfig> layers = {
      {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
       .output_gain_is_present_flag = false,
       .recon_gain_is_present_flag = false,
       .substream_count = 1,
       .coupled_substream_count = 1},
      {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_ch,
       .output_gain_is_present_flag = false,
       .recon_gain_is_present_flag = false,
       .substream_count = 3,
       .coupled_substream_count = 1},
  };
  const std::vector<DecodedUleb128> substream_ids = {10, 11, 12, 13};
  auto element = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kAudioElementId, /*reserved=*/0, kCodecConfigId,
      substream_ids,
      ScalableChannelLayoutConfig{.channel_audio_layer_configs = layers});
  ASSERT_THAT(element, IsOk());
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
       LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0},
      mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  obus.push_back(&*element);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(substream_ids.front(), &data);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->audio_elements.size(), 1);
  const auto& ae = report->audio_elements.front();
  ASSERT_EQ(ae.scalable_layers.size(), 2u);
  EXPECT_EQ(ae.channel_layout.value_or(""), "5.1");
  EXPECT_EQ(ae.channel_layout_raw.value_or(0), 2);  // LOUDSPEAKER_LAYOUT_5_1.
  ASSERT_TRUE(ae.num_channels.has_value());
  EXPECT_EQ(*ae.num_channels, 6u);
}

// ---------- Temporal-unit scan tests ----------

// Builds a `MixGainParamDefinition` matching the one inserted into the mix
// presentation by `AddMixPresentationObuWithAudioElementIds` so we can author
// parameter blocks against the same parameter ID.
MixGainParamDefinition MakeMixGainParamDefinitionMode1(
    uint32_t parameter_id, uint32_t parameter_rate) {
  // No schedule => `param_definition_mode` of 1; `default_mix_gain_` defaults
  // to Q7.8 zero.
  return MixGainParamDefinition(
      {.parameter_id = parameter_id, .parameter_rate = parameter_rate});
}

// Builds a Parameter Block OBU with a single mix-gain subblock.
std::unique_ptr<ParameterBlockObu> MakeMixGainBlock(
    const MixGainParamDefinition& def, DecodedUleb128 duration,
    std::variant<AnimationStepInt16, AnimationLinearInt16, AnimationBezierInt16>
        anim_data) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(
      /*duration=*/duration, /*constant_subblock_duration=*/duration);
  CHECK_OK(schedule);
  auto block = ParameterBlockObu::CreateMode1(ObuHeader(), def, *schedule);
  block->subblocks_.front() = std::make_unique<MixGainParameterData>(anim_data);
  return block;
}

TEST(Probe, TemporalUnitScanDisabledByDefault) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  EXPECT_FALSE(report->temporal_unit_scan.has_value());
}

TEST(Probe, TemporalUnitScanCountsFramesAndComputesDuration) {
  // `AddOpusCodecConfigWithId` uses 8 samples per frame at 48 kHz. One frame is
  // appended by `SerializeMinimalIaSequence`; add a second.
  constexpr uint32_t kNumSamplesPerFrame = 8;
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.audio_frame_count, 2u);
  EXPECT_EQ(s.temporal_unit_count, 2u);
  EXPECT_EQ(s.parameter_block_count, 0u);
  EXPECT_EQ(s.stopped_reason, "eof");
  ASSERT_EQ(s.audio_frames_by_substream.size(), 1u);
  EXPECT_EQ(s.audio_frames_by_substream.front().substream_id, kSubstreamId);
  EXPECT_EQ(s.audio_frames_by_substream.front().frame_count, 2u);
  EXPECT_EQ(s.audio_frames_by_substream.front().total_samples,
            2u * kNumSamplesPerFrame);
  ASSERT_TRUE(s.total_samples.has_value());
  EXPECT_EQ(*s.total_samples, 2u * kNumSamplesPerFrame);
  ASSERT_TRUE(s.duration_seconds.has_value());
  EXPECT_NEAR(*s.duration_seconds, (2.0 * kNumSamplesPerFrame) / 48000.0, 1e-9);
  // Per-TU index: one entry per TU, contiguous in the raw-sample timeline.
  ASSERT_EQ(s.temporal_units.size(), 2u);
  EXPECT_EQ(s.temporal_units[0].start_timestamp, 0u);
  EXPECT_EQ(s.temporal_units[0].num_samples, kNumSamplesPerFrame);
  EXPECT_EQ(s.temporal_units[0].byte_offset_from_scan_start, 0u);
  // The synthetic fixture uses `AppendAudioFrame` defaults; no trim.
  EXPECT_EQ(s.temporal_units[0].samples_to_trim_at_start, 0u);
  EXPECT_EQ(s.temporal_units[0].samples_to_trim_at_end, 0u);
  EXPECT_EQ(s.temporal_units[1].start_timestamp, kNumSamplesPerFrame);
  EXPECT_EQ(s.temporal_units[1].num_samples, kNumSamplesPerFrame);
  EXPECT_GT(s.temporal_units[1].byte_offset_from_scan_start, 0u);
  EXPECT_LT(s.temporal_units[1].byte_offset_from_scan_start, s.bytes_consumed);
}

TEST(Probe, TemporalUnitScanWithoutDetailsKeepsCountsAndDuration) {
  constexpr uint32_t kNumSamplesPerFrame = 8;
  const auto mix_gain_def =
      MakeMixGainParamDefinitionMode1(kParameterId, kParameterRate);
  auto block = MakeMixGainBlock(mix_gain_def, /*duration=*/64,
                                AnimationStepInt16{.start_point_value = 256});

  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto block_bytes = SerializeObusExpectOk({block.get()});
  data.insert(data.end(), block_bytes.begin(), block_bytes.end());
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanCounts;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  // Counts, totals, and duration match what a detailed scan reports.
  EXPECT_EQ(s.audio_frame_count, 2u);
  EXPECT_EQ(s.temporal_unit_count, 2u);
  EXPECT_EQ(s.parameter_block_count, 1u);
  EXPECT_EQ(s.parameter_block_parse_errors, 0u);
  EXPECT_EQ(s.audio_frame_parse_errors, 0u);
  ASSERT_EQ(s.audio_frames_by_substream.size(), 1u);
  EXPECT_EQ(s.audio_frames_by_substream.front().total_samples,
            2u * kNumSamplesPerFrame);
  ASSERT_TRUE(s.total_samples.has_value());
  EXPECT_EQ(*s.total_samples, 2u * kNumSamplesPerFrame);
  ASSERT_TRUE(s.duration_seconds.has_value());
  EXPECT_NEAR(*s.duration_seconds, (2.0 * kNumSamplesPerFrame) / 48000.0, 1e-9);
  // The per-TU index and parameter-block contents are not collected.
  EXPECT_TRUE(s.temporal_units.empty());
  EXPECT_TRUE(s.parameter_blocks.empty());
  EXPECT_EQ(s.stopped_reason, "eof");
}

TEST(Probe, TemporalUnitScanExtractsMixGainStepLinearAndBezier) {
  const auto mix_gain_def =
      MakeMixGainParamDefinitionMode1(kParameterId, kParameterRate);

  // Step.
  auto step_block =
      MakeMixGainBlock(mix_gain_def, /*duration=*/64,
                       AnimationStepInt16{.start_point_value = 256});
  // Linear.
  auto linear_block = MakeMixGainBlock(
      mix_gain_def, /*duration=*/64,
      AnimationLinearInt16{.start_point_value = 10, .end_point_value = 20});
  // Bezier.
  auto bezier_block = MakeMixGainBlock(
      mix_gain_def, /*duration=*/64,
      AnimationBezierInt16{.start_point_value = -32,
                           .end_point_value = 128,
                           .control_point_value = 48,
                           .control_point_relative_time = 200});

  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto step_bytes = SerializeObusExpectOk(
      {step_block.get(), linear_block.get(), bezier_block.get()});
  data.insert(data.end(), step_bytes.begin(), step_bytes.end());

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  ASSERT_EQ(s.parameter_blocks.size(), 3u);

  // Step.
  {
    const auto& b = s.parameter_blocks[0];
    EXPECT_EQ(b.parameter_id, kParameterId);
    EXPECT_EQ(b.param_type, "mix_gain");
    EXPECT_EQ(b.duration, 64u);
    ASSERT_EQ(b.subblocks.size(), 1u);
    ASSERT_TRUE(b.subblocks.front().mix_gain.has_value());
    const auto& mg = *b.subblocks.front().mix_gain;
    EXPECT_EQ(mg.animation_type, "step");
    EXPECT_EQ(mg.start_point_value_q7_8, 256);
    EXPECT_FLOAT_EQ(mg.start_point_value, 1.0f);
    EXPECT_FALSE(mg.end_point_value_q7_8.has_value());
    EXPECT_FALSE(mg.end_point_value.has_value());
  }
  // Linear.
  {
    const auto& b = s.parameter_blocks[1];
    ASSERT_EQ(b.subblocks.size(), 1u);
    ASSERT_TRUE(b.subblocks.front().mix_gain.has_value());
    const auto& mg = *b.subblocks.front().mix_gain;
    EXPECT_EQ(mg.animation_type, "linear");
    EXPECT_EQ(mg.start_point_value_q7_8, 10);
    ASSERT_TRUE(mg.end_point_value_q7_8.has_value());
    EXPECT_EQ(*mg.end_point_value_q7_8, 20);
    ASSERT_TRUE(mg.end_point_value.has_value());
    EXPECT_FLOAT_EQ(*mg.end_point_value, 20.0f / 256.0f);
  }
  // Bezier.
  {
    const auto& b = s.parameter_blocks[2];
    ASSERT_EQ(b.subblocks.size(), 1u);
    ASSERT_TRUE(b.subblocks.front().mix_gain.has_value());
    const auto& mg = *b.subblocks.front().mix_gain;
    EXPECT_EQ(mg.animation_type, "bezier");
    EXPECT_EQ(mg.start_point_value_q7_8, -32);
    EXPECT_FLOAT_EQ(mg.start_point_value, -32.0f / 256.0f);
    ASSERT_TRUE(mg.end_point_value_q7_8.has_value());
    EXPECT_EQ(*mg.end_point_value_q7_8, 128);
    ASSERT_TRUE(mg.control_point_value_q7_8.has_value());
    EXPECT_EQ(*mg.control_point_value_q7_8, 48);
    EXPECT_FLOAT_EQ(*mg.control_point_value, 48.0f / 256.0f);
    ASSERT_TRUE(mg.control_point_relative_time.has_value());
    EXPECT_EQ(*mg.control_point_relative_time, 200);
  }

  // Timestamps advance by `duration` per block for a given parameter id.
  EXPECT_EQ(s.parameter_blocks[0].start_timestamp, 0u);
  EXPECT_EQ(s.parameter_blocks[0].end_timestamp, 64u);
  EXPECT_EQ(s.parameter_blocks[1].start_timestamp, 64u);
  EXPECT_EQ(s.parameter_blocks[1].end_timestamp, 128u);
  EXPECT_EQ(s.parameter_blocks[2].start_timestamp, 128u);
  EXPECT_EQ(s.parameter_blocks[2].end_timestamp, 192u);
}

TEST(Probe, TemporalUnitScanStopsOnNextIaSequenceHeader) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  // Append a second (non-redundant) IA Sequence Header. The scan should stop
  // cleanly before consuming it so a subsequent caller can start a new scan.
  const IASequenceHeaderObu second_sequence(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  const auto second_bytes = SerializeObusExpectOk({&second_sequence});
  data.insert(data.end(), second_bytes.begin(), second_bytes.end());

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  EXPECT_EQ(report->temporal_unit_scan->stopped_reason, "next_ia_sequence");
}

TEST(Probe, TemporalUnitScanHandlesTruncation) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  // Append an audio frame then lop off the last byte to simulate truncation.
  AppendAudioFrame(kSubstreamId, &data);
  data.pop_back();

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  EXPECT_EQ(report->temporal_unit_scan->stopped_reason, "truncated");
}

TEST(Probe, TemporalUnitScanHasZeroParseErrorsWhenHealthy) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  EXPECT_EQ(report->temporal_unit_scan->audio_frame_parse_errors, 0u);
  EXPECT_EQ(report->temporal_unit_scan->parameter_block_parse_errors, 0u);
}

TEST(Probe, TemporalUnitScanCountsUnknownParameterIdAsParseError) {
  constexpr DecodedUleb128 kUnknownParameterId = 12345;
  ASSERT_NE(kUnknownParameterId, kParameterId);

  // Build a parameter block whose parameter_id is not declared in the
  // descriptor OBUs. The probe should skip the block and bump
  // `parameter_block_parse_errors` rather than silently drop it.
  const auto stray_def =
      MakeMixGainParamDefinitionMode1(kUnknownParameterId, kParameterRate);
  auto stray_block = MakeMixGainBlock(
      stray_def, /*duration=*/64, AnimationStepInt16{.start_point_value = 0});

  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto stray_bytes = SerializeObusExpectOk({stray_block.get()});
  data.insert(data.end(), stray_bytes.begin(), stray_bytes.end());

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.parameter_block_count, 0u);
  EXPECT_EQ(s.parameter_block_parse_errors, 1u);
  EXPECT_TRUE(s.parameter_blocks.empty());
  EXPECT_EQ(s.stopped_reason, "eof");
}

TEST(Probe, TemporalUnitScanCountsTemporalDelimiters) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  const TemporalDelimiterObu delimiter((ObuHeader()));
  const auto delimiter_bytes = SerializeObusExpectOk({&delimiter});
  data.insert(data.end(), delimiter_bytes.begin(), delimiter_bytes.end());
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.temporal_delimiter_count, 1u);
  EXPECT_EQ(s.audio_frame_count,
            2u);  // One from SerializeMinimal, one appended.
  EXPECT_EQ(s.stopped_reason, "eof");
}

TEST(Probe, TemporalUnitScanTracksMultipleSubstreams) {
  constexpr DecodedUleb128 kSubstreamIdA = 11;
  constexpr DecodedUleb128 kSubstreamIdB = 22;

  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                {kSubstreamIdA, kSubstreamIdB},
                                                codec_configs, audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithAudioElementIds(kMixPresentationId,
                                           {kAudioElementId}, kParameterId,
                                           kParameterRate, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  // Interleave two frames from A with one from B.
  AppendAudioFrame(kSubstreamIdA, &data);
  AppendAudioFrame(kSubstreamIdB, &data);
  AppendAudioFrame(kSubstreamIdA, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.audio_frame_count, 3u);
  // TU count is bounded by the per-substream maximum.
  EXPECT_EQ(s.temporal_unit_count, 2u);

  ASSERT_EQ(s.audio_frames_by_substream.size(), 2u);
  // Substream ids are reported in sorted order.
  EXPECT_EQ(s.audio_frames_by_substream[0].substream_id, kSubstreamIdA);
  EXPECT_EQ(s.audio_frames_by_substream[0].frame_count, 2u);
  EXPECT_EQ(s.audio_frames_by_substream[1].substream_id, kSubstreamIdB);
  EXPECT_EQ(s.audio_frames_by_substream[1].frame_count, 1u);
}

TEST(Probe, TemporalUnitScanCancelledByCallback) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  int calls = 0;
  options.should_continue = [&calls](const ScanProgress&) {
    // Allow the first OBU through, then cancel.
    return ++calls <= 1;
  };
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.stopped_reason, "cancelled");
  // Partial results remain valid: exactly one OBU was processed.
  EXPECT_EQ(s.audio_frame_count, 1u);
  EXPECT_EQ(calls, 2);
}

TEST(Probe, TemporalUnitScanReportsProgressToCallback) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  ScanProgress last_progress;
  int calls = 0;
  options.should_continue = [&](const ScanProgress& progress) {
    last_progress = progress;
    ++calls;
    return true;
  };
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.stopped_reason, "eof");
  // One call per OBU plus the final EOF-detecting iteration.
  EXPECT_GT(calls, 1);
  // The last call saw all the bytes the scan ultimately consumed and the
  // first TU already finalized (a repeat substream closes the previous TU).
  EXPECT_EQ(last_progress.bytes_consumed, s.bytes_consumed);
  EXPECT_EQ(last_progress.temporal_unit_count, 1u);
}

TEST(Probe, TemporalUnitScanRespectsIterationBudget) {
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  options.max_scan_obu_iterations = 1;
  const auto report = Probe(absl::MakeConstSpan(data), options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.stopped_reason, "scan_budget_exceeded");
  // Only the single budgeted iteration ran.
  EXPECT_EQ(s.audio_frame_count, 1u);
}

// ---------- Descriptor coverage ----------

TEST(Probe, ReportsFlacCodecConfig) {
  constexpr uint32_t kNumSamplesPerFrame = 16;
  constexpr uint32_t kSampleRate = 48000;
  constexpr uint8_t kBitsPerSampleMinusOne = 15;  // 16-bit samples.

  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddFlacCodecConfigWithId(kCodecConfigId, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {kSubstreamId},
      codec_configs, audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(kSubstreamId, &data);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->codec_configs.size(), 1);
  const auto& cc = report->codec_configs.front();
  EXPECT_EQ(cc.codec_id, "FLAC");
  EXPECT_EQ(cc.num_samples_per_frame, kNumSamplesPerFrame);
  ASSERT_TRUE(cc.flac.has_value());
  ASSERT_FALSE(cc.flac->metadata_blocks.empty());
  const auto& sinfo = cc.flac->metadata_blocks.front();
  EXPECT_EQ(sinfo.block_type, "StreamInfo");
  EXPECT_TRUE(sinfo.is_stream_info);
  EXPECT_EQ(sinfo.sample_rate, kSampleRate);
  EXPECT_EQ(sinfo.bits_per_sample, kBitsPerSampleMinusOne);
  EXPECT_EQ(sinfo.minimum_block_size, kNumSamplesPerFrame);
  EXPECT_EQ(sinfo.maximum_block_size, kNumSamplesPerFrame);
  // The fixture's STREAMINFO carries no sample total, so no
  // descriptors-only duration is available.
  EXPECT_FALSE(report->descriptor_total_samples.has_value());
  EXPECT_FALSE(report->descriptor_duration_seconds.has_value());
}

TEST(Probe, ReportsDescriptorDurationFromFlacStreamInfo) {
  constexpr uint32_t kNumSamplesPerFrame = 16;
  constexpr uint32_t kSampleRate = 48000;
  constexpr uint64_t kTotalSamples = 96000;  // Two seconds at 48 kHz.

  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  auto flac_obu = CodecConfigObu::Create(
      ObuHeader(), kCodecConfigId,
      {.codec_id = CodecConfig::kCodecIdFlac,
       .num_samples_per_frame = kNumSamplesPerFrame,
       .decoder_config = FlacDecoderConfig(
           {{{.header = {.block_type = FlacMetaBlockHeader::kFlacStreamInfo},
              .payload = FlacMetaBlockStreamInfo{
                  .minimum_block_size =
                      static_cast<uint16_t>(kNumSamplesPerFrame),
                  .maximum_block_size =
                      static_cast<uint16_t>(kNumSamplesPerFrame),
                  .sample_rate = kSampleRate,
                  .bits_per_sample = 15,
                  .total_samples_in_stream = kTotalSamples}}}})});
  ASSERT_THAT(flac_obu, IsOk());
  codec_configs.emplace(kCodecConfigId, *std::move(flac_obu));
  DescriptorObus::AudioElementsById audio_elements;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {kSubstreamId},
      codec_configs, audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(kSubstreamId, &data);

  // Descriptors-only probe: the duration arrives without any scan.
  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  EXPECT_FALSE(report->temporal_unit_scan.has_value());
  ASSERT_TRUE(report->descriptor_total_samples.has_value());
  EXPECT_EQ(*report->descriptor_total_samples, kTotalSamples);
  ASSERT_TRUE(report->descriptor_duration_seconds.has_value());
  EXPECT_NEAR(*report->descriptor_duration_seconds, 2.0, 1e-9);
}

TEST(Probe, ReportsAacCodecConfig) {
  constexpr uint32_t kNumSamplesPerFrame = 1024;

  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddAacCodecConfig(kCodecConfigId, kNumSamplesPerFrame,
                    AudioSpecificConfig::SampleFrequencyIndex::k48000,
                    codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {kSubstreamId},
      codec_configs, audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(kSubstreamId, &data);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  ASSERT_EQ(report->codec_configs.size(), 1);
  const auto& cc = report->codec_configs.front();
  EXPECT_EQ(cc.codec_id, "AAC LC");
  EXPECT_EQ(cc.num_samples_per_frame, kNumSamplesPerFrame);
  ASSERT_TRUE(cc.aac.has_value());
  EXPECT_EQ(cc.aac->audio_object_type, AudioSpecificConfig::kAudioObjectType);
  EXPECT_EQ(cc.aac->channel_configuration,
            AudioSpecificConfig::kChannelConfiguration);
  EXPECT_EQ(cc.aac->sample_frequency_index, "48000");
}

TEST(Probe, ReportsReservedAndBaseEnhancedProfiles) {
  const IASequenceHeaderObu sequence_header(
      ObuHeader(), ProfileVersion::kIamfBaseEnhancedProfile,
      ProfileVersion::kIamfReserved255Profile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                {kSubstreamId}, codec_configs,
                                                audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithAudioElementIds(kMixPresentationId,
                                           {kAudioElementId}, kParameterId,
                                           kParameterRate, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  AppendAudioFrame(kSubstreamId, &data);

  const auto report = Probe(absl::MakeConstSpan(data));
  ASSERT_THAT(report, IsOk());
  EXPECT_EQ(report->primary_profile, "base_enhanced");
  EXPECT_EQ(report->additional_profile, "reserved_255");
}

// ---------- ProbeFile ----------

// Writes `data` to a fresh file under the test temp dir and returns its path.
std::string WriteTempIamfFile(const std::string& name,
                              absl::Span<const uint8_t> data) {
  const std::string path = absl::StrCat(::testing::TempDir(), "/", name);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  CHECK(out.good());
  return path;
}

TEST(ProbeFile, MatchesProbeOnDescriptors) {
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto path = WriteTempIamfFile("probe_file_descriptors.iamf", data);

  const auto from_file = ProbeFile(path);
  ASSERT_THAT(from_file, IsOk());
  EXPECT_EQ(from_file->primary_profile, "simple");
  EXPECT_EQ(from_file->additional_profile, "base");
  ASSERT_EQ(from_file->codec_configs.size(), 1u);
  EXPECT_EQ(from_file->codec_configs.front().codec_id, "Opus");
  EXPECT_EQ(from_file->descriptor_bytes_consumed, descriptor_size);
  EXPECT_FALSE(from_file->temporal_unit_scan.has_value());
}

TEST(ProbeFile, ScansTemporalUnits) {
  constexpr uint32_t kNumSamplesPerFrame = 8;
  size_t descriptor_size = 0;
  auto data = SerializeMinimalIaSequence(&descriptor_size);
  AppendAudioFrame(kSubstreamId, &data);
  const auto path = WriteTempIamfFile("probe_file_scan.iamf", data);

  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const auto report = ProbeFile(path, options);
  ASSERT_THAT(report, IsOk());
  ASSERT_TRUE(report->temporal_unit_scan.has_value());
  const auto& s = *report->temporal_unit_scan;
  EXPECT_EQ(s.audio_frame_count, 2u);
  EXPECT_EQ(s.temporal_unit_count, 2u);
  EXPECT_EQ(s.stopped_reason, "eof");
  ASSERT_TRUE(s.total_samples.has_value());
  EXPECT_EQ(*s.total_samples, 2u * kNumSamplesPerFrame);
  ASSERT_EQ(s.temporal_units.size(), 2u);
}

TEST(ProbeFile, MissingFileReportsNotFound) {
  EXPECT_THAT(
      ProbeFile(absl::StrCat(::testing::TempDir(), "/does_not_exist.iamf"))
          .status(),
      StatusIs(absl::StatusCode::kNotFound));
}

TEST(ProbeFile, TruncatedDescriptorsReportInvalidArgument) {
  // Unlike the span-based `Probe`, the whole file is visible: truncation
  // cannot be cured by feeding more bytes, so it is invalid input.
  size_t descriptor_size = 0;
  const auto data = SerializeMinimalIaSequence(&descriptor_size);
  const auto path = WriteTempIamfFile(
      "probe_file_truncated.iamf",
      absl::MakeConstSpan(data).subspan(0, descriptor_size / 2));
  EXPECT_THAT(ProbeFile(path).status(),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace iamf_tools
