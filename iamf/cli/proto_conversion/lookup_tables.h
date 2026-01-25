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
#ifndef CLI_LOOKUP_TABLES_H_
#define CLI_LOOKUP_TABLES_H_

#include <array>
#include <utility>

#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

/*!\brief Backing data for lookup tables.
 *
 * This class stores `inline static constexpr` pairs of values, which are
 * guaranteed to only have a single copy in the program.
 *
 * This data backs the creation of run-time lookup tables using
 * `ObuUtil::BuildStaticMapFromPairs`. Or the inverted version of those lookup
 * tables using `BuildStaticMapFromInvertedPairs`.
 */
class LookupTables {
 public:
  inline static constexpr auto kProtoAndInternalProfileVersions = []() {
    using enum iamf_tools_cli_proto::ProfileVersion;
    using enum ProfileVersion;
    return std::to_array<
        std::pair<iamf_tools_cli_proto::ProfileVersion, ProfileVersion>>({
        {PROFILE_VERSION_SIMPLE, kIamfSimpleProfile},
        {PROFILE_VERSION_BASE, kIamfBaseProfile},
        {PROFILE_VERSION_BASE_ENHANCED, kIamfBaseEnhancedProfile},
        {PROFILE_VERSION_BASE_ADVANCED, kIamfBaseAdvancedProfile},
        {PROFILE_VERSION_ADVANCED1, kIamfAdvanced1Profile},
        {PROFILE_VERSION_ADVANCED2, kIamfAdvanced2Profile},
        {PROFILE_VERSION_RESERVED_255, kIamfReserved255Profile},
    });
  }();

  inline static constexpr auto kProtoAndInternalDMixPModes = []() {
    using enum iamf_tools_cli_proto::DMixPMode;
    using enum DemixingInfoParameterData::DMixPMode;
    return std::to_array<std::pair<iamf_tools_cli_proto::DMixPMode,
                                   DemixingInfoParameterData::DMixPMode>>(
        {{DMIXP_MODE_1, kDMixPMode1},
         {DMIXP_MODE_2, kDMixPMode2},
         {DMIXP_MODE_3, kDMixPMode3},
         {DMIXP_MODE_RESERVED_A, kDMixPModeReserved1},
         {DMIXP_MODE_1_N, kDMixPMode1_n},
         {DMIXP_MODE_2_N, kDMixPMode2_n},
         {DMIXP_MODE_3_N, kDMixPMode3_n},
         {DMIXP_MODE_RESERVED_B, kDMixPModeReserved2}});
  }();

  inline static constexpr auto kProtoAndInternalCodecIds = []() {
    using enum iamf_tools_cli_proto::CodecId;
    using enum CodecConfig::CodecId;
    return std::to_array<
        std::pair<iamf_tools_cli_proto::CodecId, CodecConfig::CodecId>>({
        {CODEC_ID_OPUS, kCodecIdOpus},
        {CODEC_ID_FLAC, kCodecIdFlac},
        {CODEC_ID_AAC_LC, kCodecIdAacLc},
        {CODEC_ID_LPCM, kCodecIdLpcm},
    });
  }();

  inline static constexpr auto kProtoAndInternalFlacBlockTypes = []() {
    using enum iamf_tools_cli_proto::FlacBlockType;
    using enum FlacMetaBlockHeader::FlacBlockType;
    return std::to_array<std::pair<iamf_tools_cli_proto::FlacBlockType,
                                   FlacMetaBlockHeader::FlacBlockType>>(
        {{FLAC_BLOCK_TYPE_STREAMINFO, kFlacStreamInfo},
         {FLAC_BLOCK_TYPE_PADDING, kFlacPadding},
         {FLAC_BLOCK_TYPE_APPLICATION, kFlacApplication},
         {FLAC_BLOCK_TYPE_SEEKTABLE, kFlacSeektable},
         {FLAC_BLOCK_TYPE_VORBIS_COMMENT, kFlacVorbisComment},
         {FLAC_BLOCK_TYPE_CUESHEET, kFlacCuesheet},
         {FLAC_BLOCK_TYPE_PICTURE, kFlacPicture}});
  }();

  inline static constexpr auto kProtoAndInternalSampleFrequencyIndices = []() {
    using enum iamf_tools_cli_proto::SampleFrequencyIndex;
    using enum AudioSpecificConfig::SampleFrequencyIndex;
    return std::to_array<std::pair<iamf_tools_cli_proto::SampleFrequencyIndex,
                                   AudioSpecificConfig::SampleFrequencyIndex>>(
        {{AAC_SAMPLE_FREQUENCY_INDEX_96000, k96000},
         {AAC_SAMPLE_FREQUENCY_INDEX_88200, k88200},
         {AAC_SAMPLE_FREQUENCY_INDEX_64000, k64000},
         {AAC_SAMPLE_FREQUENCY_INDEX_48000, k48000},
         {AAC_SAMPLE_FREQUENCY_INDEX_44100, k44100},
         {AAC_SAMPLE_FREQUENCY_INDEX_32000, k32000},
         {AAC_SAMPLE_FREQUENCY_INDEX_24000, k24000},
         {AAC_SAMPLE_FREQUENCY_INDEX_22050, k22050},
         {AAC_SAMPLE_FREQUENCY_INDEX_16000, k16000},
         {AAC_SAMPLE_FREQUENCY_INDEX_12000, k12000},
         {AAC_SAMPLE_FREQUENCY_INDEX_11025, k11025},
         {AAC_SAMPLE_FREQUENCY_INDEX_8000, k8000},
         {AAC_SAMPLE_FREQUENCY_INDEX_7350, k7350},
         {AAC_SAMPLE_FREQUENCY_INDEX_RESERVED_A, kReservedA},
         {AAC_SAMPLE_FREQUENCY_INDEX_RESERVED_B, kReservedB}});
  }();

  inline static constexpr auto kProtoAndInternalLoudspeakerLayouts = []() {
    using enum iamf_tools_cli_proto::LoudspeakerLayout;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    return std::to_array<std::pair<iamf_tools_cli_proto::LoudspeakerLayout,
                                   ChannelAudioLayerConfig::LoudspeakerLayout>>(
        {
            {LOUDSPEAKER_LAYOUT_MONO, kLayoutMono},
            {LOUDSPEAKER_LAYOUT_STEREO, kLayoutStereo},
            {LOUDSPEAKER_LAYOUT_5_1_CH, kLayout5_1_ch},
            {LOUDSPEAKER_LAYOUT_5_1_2_CH, kLayout5_1_2_ch},
            {LOUDSPEAKER_LAYOUT_5_1_4_CH, kLayout5_1_4_ch},
            {LOUDSPEAKER_LAYOUT_7_1_CH, kLayout7_1_ch},
            {LOUDSPEAKER_LAYOUT_7_1_2_CH, kLayout7_1_2_ch},
            {LOUDSPEAKER_LAYOUT_7_1_4_CH, kLayout7_1_4_ch},
            {LOUDSPEAKER_LAYOUT_3_1_2_CH, kLayout3_1_2_ch},
            {LOUDSPEAKER_LAYOUT_BINAURAL, kLayoutBinaural},
            {LOUDSPEAKER_LAYOUT_RESERVED_10, kLayoutReserved10},
            {LOUDSPEAKER_LAYOUT_RESERVED_14, kLayoutReserved14},
            {LOUDSPEAKER_LAYOUT_EXPANDED, kLayoutExpanded},
        });
  }();

  inline static constexpr auto kProtoAndInternalExpandedLoudspeakerLayouts =
      []() {
        using enum iamf_tools_cli_proto::ExpandedLoudspeakerLayout;
        using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
        return std::to_array<
            std::pair<iamf_tools_cli_proto::ExpandedLoudspeakerLayout,
                      ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>>({
            {EXPANDED_LOUDSPEAKER_LAYOUT_LFE, kExpandedLayoutLFE},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_S, kExpandedLayoutStereoS},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_SS, kExpandedLayoutStereoSS},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_RS, kExpandedLayoutStereoRS},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TF, kExpandedLayoutStereoTF},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TB, kExpandedLayoutStereoTB},
            {EXPANDED_LOUDSPEAKER_LAYOUT_TOP_4_CH, kExpandedLayoutTop4Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_3_0_CH, kExpandedLayout3_0_ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_9_1_6_CH, kExpandedLayout9_1_6_ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_F, kExpandedLayoutStereoF},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_SI, kExpandedLayoutStereoSi},
            {EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TP_SI,
             kExpandedLayoutStereoTpSi},
            {EXPANDED_LOUDSPEAKER_LAYOUT_TOP_6_CH, kExpandedLayoutTop6Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_10_2_9_3_CH, kExpandedLayout10_2_9_3},
            {EXPANDED_LOUDSPEAKER_LAYOUT_LFE_PAIR, kExpandedLayoutLfePair},
            {EXPANDED_LOUDSPEAKER_LAYOUT_BOTTOM_3_CH, kExpandedLayoutBottom3Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_7_1_5_4_CH, kExpandedLayout7_1_5_4Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_BOTTOM_4_CH, kExpandedLayoutBottom4Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_TOP_1_CH, kExpandedLayoutTop1Ch},
            {EXPANDED_LOUDSPEAKER_LAYOUT_TOP_5_CH, kExpandedLayoutTop5Ch},
        });
      }();

  inline static constexpr auto kProtoAndInternalSoundSystems = []() {
    using enum iamf_tools_cli_proto::SoundSystem;
    using enum LoudspeakersSsConventionLayout::SoundSystem;
    return std::to_array<
        std::pair<iamf_tools_cli_proto::SoundSystem,
                  LoudspeakersSsConventionLayout::SoundSystem>>({
        {SOUND_SYSTEM_A_0_2_0, kSoundSystemA_0_2_0},
        {SOUND_SYSTEM_B_0_5_0, kSoundSystemB_0_5_0},
        {SOUND_SYSTEM_C_2_5_0, kSoundSystemC_2_5_0},
        {SOUND_SYSTEM_D_4_5_0, kSoundSystemD_4_5_0},
        {SOUND_SYSTEM_E_4_5_1, kSoundSystemE_4_5_1},
        {SOUND_SYSTEM_F_3_7_0, kSoundSystemF_3_7_0},
        {SOUND_SYSTEM_G_4_9_0, kSoundSystemG_4_9_0},
        {SOUND_SYSTEM_H_9_10_3, kSoundSystemH_9_10_3},
        {SOUND_SYSTEM_I_0_7_0, kSoundSystemI_0_7_0},
        {SOUND_SYSTEM_J_4_7_0, kSoundSystemJ_4_7_0},
        {SOUND_SYSTEM_10_2_7_0, kSoundSystem10_2_7_0},
        {SOUND_SYSTEM_11_2_3_0, kSoundSystem11_2_3_0},
        {SOUND_SYSTEM_12_0_1_0, kSoundSystem12_0_1_0},
        {SOUND_SYSTEM_13_6_9_0, kSoundSystem13_6_9_0},
        {SOUND_SYSTEM_14_5_7_4, kSoundSystem14_5_7_4},
    });
  }();

  inline static constexpr auto kProtoAndInternalInfoTypeBitmasks = []() {
    using enum iamf_tools_cli_proto::LoudnessInfoTypeBitMask;
    using enum LoudnessInfo::InfoTypeBitmask;
    return std::to_array<
        std::pair<iamf_tools_cli_proto::LoudnessInfoTypeBitMask,
                  LoudnessInfo::InfoTypeBitmask>>(
        {{LOUDNESS_INFO_TYPE_TRUE_PEAK, kTruePeak},
         {LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS, kAnchoredLoudness},
         {LOUDNESS_INFO_TYPE_LIVE, kLive},
         {LOUDNESS_INFO_TYPE_RESERVED_8, kInfoTypeBitMask8},
         {LOUDNESS_INFO_TYPE_RESERVED_16, kInfoTypeBitMask16},
         {LOUDNESS_INFO_TYPE_RESERVED_32, kInfoTypeBitMask32},
         {LOUDNESS_INFO_TYPE_RESERVED_64, kInfoTypeBitMask64},
         {LOUDNESS_INFO_TYPE_RESERVED_128, kInfoTypeBitMask128}});
  }();

  inline static constexpr auto kProtoAndInternalPreferredLoudspeakerRenderer =
      []() {
        using enum iamf_tools_cli_proto::PreferredLoudspeakerRenderer;
        using enum PreferredLoudspeakerRenderer;
        return std::to_array<
            std::pair<iamf_tools_cli_proto::PreferredLoudspeakerRenderer,
                      PreferredLoudspeakerRenderer>>(
            {{PREFERRED_LOUDSPEAKER_RENDERER_NONE, kNone},
             {PREFERRED_LOUDSPEAKER_RENDERER_RESERVED_255, kReservedEnd}});
      }();

  inline static constexpr auto kProtoAndInternalPreferredBinauralRenderer =
      []() {
        using enum iamf_tools_cli_proto::PreferredBinauralRenderer;
        using enum PreferredBinauralRenderer;
        return std::to_array<
            std::pair<iamf_tools_cli_proto::PreferredBinauralRenderer,
                      PreferredBinauralRenderer>>(
            {{PREFERRED_BINAURAL_RENDERER_NONE, kNone},
             {PREFERRED_BINAURAL_RENDERER_RESERVED_255, kReservedEnd}});
      }();

  inline static constexpr auto kProtoArbitraryObuTypeAndInternalObuTypes =
      []() {
        using enum iamf_tools_cli_proto::ArbitraryObuType;
        return std::to_array<
            std::pair<iamf_tools_cli_proto::ArbitraryObuType, ObuType>>(
            {{OBU_IA_CODEC_CONFIG, kObuIaCodecConfig},
             {OBU_IA_AUDIO_ELEMENT, kObuIaAudioElement},
             {OBU_IA_MIX_PRESENTATION, kObuIaMixPresentation},
             {OBU_IA_PARAMETER_BLOCK, kObuIaParameterBlock},
             {OBU_IA_TEMPORAL_DELIMITER, kObuIaTemporalDelimiter},
             {OBU_IA_AUDIO_FRAME, kObuIaAudioFrame},
             {OBU_IA_AUDIO_FRAME_ID_0, kObuIaAudioFrameId0},
             {OBU_IA_AUDIO_FRAME_ID_1, kObuIaAudioFrameId1},
             {OBU_IA_AUDIO_FRAME_ID_2, kObuIaAudioFrameId2},
             {OBU_IA_AUDIO_FRAME_ID_3, kObuIaAudioFrameId3},
             {OBU_IA_AUDIO_FRAME_ID_4, kObuIaAudioFrameId4},
             {OBU_IA_AUDIO_FRAME_ID_5, kObuIaAudioFrameId5},
             {OBU_IA_AUDIO_FRAME_ID_6, kObuIaAudioFrameId6},
             {OBU_IA_AUDIO_FRAME_ID_7, kObuIaAudioFrameId7},
             {OBU_IA_AUDIO_FRAME_ID_8, kObuIaAudioFrameId8},
             {OBU_IA_AUDIO_FRAME_ID_9, kObuIaAudioFrameId9},
             {OBU_IA_AUDIO_FRAME_ID_10, kObuIaAudioFrameId10},
             {OBU_IA_AUDIO_FRAME_ID_11, kObuIaAudioFrameId11},
             {OBU_IA_AUDIO_FRAME_ID_12, kObuIaAudioFrameId12},
             {OBU_IA_AUDIO_FRAME_ID_13, kObuIaAudioFrameId13},
             {OBU_IA_AUDIO_FRAME_ID_14, kObuIaAudioFrameId14},
             {OBU_IA_AUDIO_FRAME_ID_15, kObuIaAudioFrameId15},
             {OBU_IA_AUDIO_FRAME_ID_16, kObuIaAudioFrameId16},
             {OBU_IA_AUDIO_FRAME_ID_17, kObuIaAudioFrameId17},
             {OBU_IA_METADATA, kObuIaMetadata},
             {OBU_IA_RESERVED_25, kObuIaReserved25},
             {OBU_IA_RESERVED_26, kObuIaReserved26},
             {OBU_IA_RESERVED_27, kObuIaReserved27},
             {OBU_IA_RESERVED_28, kObuIaReserved28},
             {OBU_IA_RESERVED_29, kObuIaReserved29},
             {OBU_IA_RESERVED_30, kObuIaReserved30},
             {OBU_IA_SEQUENCE_HEADER, kObuIaSequenceHeader}});
      }();
};

}  // namespace iamf_tools

#endif  // CLI_LOOKUP_TABLES_H_
