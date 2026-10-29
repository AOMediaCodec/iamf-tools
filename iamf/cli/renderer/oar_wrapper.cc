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

#include "iamf/cli/renderer/oar_wrapper.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/rendering_config.h"

extern "C" {

#include "include/animation.h"
#include "include/oar.h"
#include "include/oar_base.h"
#include "include/oar_metadata.h"

}  // extern "C"

namespace iamf_tools {

// `liboar` uses integer return values extensively, where 0 means success.
constexpr int kOarSuccess = 0;
const size_t OarWrapper::kMaxNumObjects = def_max_number_of_objects;

namespace {

absl::Status FillTargetLayout(const Layout& iamf_layout,
                              oar_layout_t& target_layout) {
  switch (iamf_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention: {
      auto sound_system_convention_layout =
          std::get_if<LoudspeakersSsConventionLayout>(
              &iamf_layout.specific_layout);
      if (sound_system_convention_layout == nullptr) {
        return absl::InvalidArgumentError(
            "Specific layout inconsistent with sound system layout type.");
      }
      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, oar_layout_t>>
          kSoundSystemToOarLayout({
              {kSoundSystemA_0_2_0, ck_oar_layout_sound_system_a_020},
              {kSoundSystemB_0_5_0, ck_oar_layout_sound_system_b_050},
              {kSoundSystemC_2_5_0, ck_oar_layout_sound_system_c_250},
              {kSoundSystemD_4_5_0, ck_oar_layout_sound_system_d_450},
              {kSoundSystemE_4_5_1, ck_oar_layout_sound_system_e_451},
              {kSoundSystemF_3_7_0, ck_oar_layout_sound_system_f_370},
              {kSoundSystemG_4_9_0, ck_oar_layout_sound_system_g_490},
              {kSoundSystemH_9_10_3, ck_oar_layout_sound_system_h_9a3},
              {kSoundSystemI_0_7_0, ck_oar_layout_sound_system_i_070},
              {kSoundSystemJ_4_7_0, ck_oar_layout_sound_system_j_470},
              {kSoundSystem10_2_7_0, ck_oar_layout_712},
              {kSoundSystem11_2_3_0, ck_oar_layout_312},
              {kSoundSystem12_0_1_0, ck_oar_layout_mono},
              {kSoundSystem13_6_9_0, ck_oar_layout_916},
              {kSoundSystem14_5_7_4, ck_oar_layout_7154},
          });

      const auto& target_layout_result =
          LookupInMap(*kSoundSystemToOarLayout,
                      sound_system_convention_layout->sound_system,
                      "OAR's layout type for `SoundSystem`");
      if (!target_layout_result.ok()) {
        return target_layout_result.status();
      }
      target_layout = *target_layout_result;
      break;
    }
    case kLayoutTypeBinaural:
      if (!std::holds_alternative<LoudspeakersReservedOrBinauralLayout>(
              iamf_layout.specific_layout)) {
        return absl::InvalidArgumentError(
            "Specific layout inconsistent with binaural layout type.");
      }
      target_layout = ck_oar_layout_binaural;
      break;
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported layout_type= ", iamf_layout.layout_type));
  }

  return absl::OkStatus();
}

absl::StatusOr<oar_layout_t> LookupOarLayoutFromScalableLoudspeakerLayout(
    const ChannelAudioLayerConfig& layer_config) {
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;
  if (layer_config.loudspeaker_layout == kLayoutExpanded) {
    if (!layer_config.expanded_loudspeaker_layout.has_value()) {
      return absl::InvalidArgumentError(
          "Expected expanded loudspeaker layout not set.");
    }

    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    static const absl::NoDestructor<absl::flat_hash_map<
        ChannelAudioLayerConfig::ExpandedLoudspeakerLayout, oar_layout_t>>
        kExpandedLoudspeakerLayoutToOarLayout({
            {kExpandedLayoutLFE, ck_oar_layout_lfe},
            {kExpandedLayoutStereoS, ck_oar_layout_stereo_s},
            {kExpandedLayoutStereoSS, ck_oar_layout_stereo_ss},
            {kExpandedLayoutStereoRS, ck_oar_layout_stereo_rs},
            {kExpandedLayoutStereoTF, ck_oar_layout_stereo_tf},
            {kExpandedLayoutStereoTB, ck_oar_layout_stereo_tb},
            {kExpandedLayoutTop4Ch, ck_oar_layout_top_4ch},
            {kExpandedLayout9_1_6_ch, ck_oar_layout_916},
            {kExpandedLayout3_0_ch, ck_oar_layout_3ch},
            {kExpandedLayoutStereoF, ck_oar_layout_stereo_f},
            {kExpandedLayoutStereoSi, ck_oar_layout_stereo_si},
            {kExpandedLayoutStereoTpSi, ck_oar_layout_stereo_tpsi},
            {kExpandedLayoutTop6Ch, ck_oar_layout_top_6ch},
            {kExpandedLayout10_2_9_3, ck_oar_layout_a293},
            {kExpandedLayoutLfePair, ck_oar_layout_lfe_pair},
            {kExpandedLayoutBottom3Ch, ck_oar_layout_bottom_3ch},
            {kExpandedLayout7_1_5_4Ch, ck_oar_layout_7154},
            {kExpandedLayoutBottom4Ch, ck_oar_layout_bottom_4ch},
            {kExpandedLayoutTop1Ch, ck_oar_layout_top_1ch},
            {kExpandedLayoutTop5Ch, ck_oar_layout_top_5ch},
        });

    return LookupInMap(*kExpandedLoudspeakerLayoutToOarLayout,
                       *layer_config.expanded_loudspeaker_layout,
                       "OAR's layout type for `ExpandedLoudspeakerLayout`");
  }
  static const absl::NoDestructor<absl::flat_hash_map<
      ChannelAudioLayerConfig::LoudspeakerLayout, oar_layout_t>>
      kLoudspeakerLayoutToOarLayout({
          {kLayoutMono, ck_oar_layout_mono},
          {kLayoutStereo, ck_oar_layout_stereo},
          {kLayout5_1_ch, ck_oar_layout_51},
          {kLayout5_1_2_ch, ck_oar_layout_512},
          {kLayout5_1_4_ch, ck_oar_layout_514},
          {kLayout7_1_ch, ck_oar_layout_71},
          {kLayout7_1_2_ch, ck_oar_layout_712},
          {kLayout7_1_4_ch, ck_oar_layout_714},
          {kLayout3_1_2_ch, ck_oar_layout_312},
          {kLayoutBinaural, ck_oar_layout_binaural},
      });

  return LookupInMap(*kLoudspeakerLayoutToOarLayout,
                     layer_config.loudspeaker_layout,
                     "OAR's layout type for `LoudspeakerLayout`");
}

absl::StatusOr<oar_hoa_t> LookupOarOrderFromAmbisonicsConfig(
    const AmbisonicsConfig& ambisonics_config) {
  const auto mode = ambisonics_config.GetAmbisonicsMode();
  if (mode != AmbisonicsConfig::kAmbisonicsModeMono &&
      mode != AmbisonicsConfig::kAmbisonicsModeProjection) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported ambisonics mode. mode= ", mode));
  }

  int ambisonics_order = 0;
  RETURN_IF_NOT_OK(GetAmbisonicsOrder(ambisonics_config.GetOutputChannelCount(),
                                      ambisonics_order));

  static const absl::NoDestructor<absl::flat_hash_map<int, oar_hoa_t>>
      kAmbisonicsOrderToOarOrder({
          {0, ck_oar_zoa},
          {1, ck_oar_1oa},
          {2, ck_oar_2oa},
          {3, ck_oar_3oa},
          {4, ck_oar_4oa},
      });
  return LookupInMap(*kAmbisonicsOrderToOarOrder, ambisonics_order,
                     "OAR's ambisonics order for `AmbisonicsConfig`");
}

absl::Status FillSceneBasedConfig(
    const AudioElementObu::AudioElementConfig& audio_element_config,
    oar_audio_element_config_t& config) {
  const auto* ambisonics_config =
      std::get_if<AmbisonicsConfig>(&audio_element_config);
  if (ambisonics_config == nullptr) {
    return absl::InvalidArgumentError(
        "Ambisonics config is inconsistent with audio element type.");
  }
  const auto& order_result =
      LookupOarOrderFromAmbisonicsConfig(*ambisonics_config);
  if (!order_result.ok()) {
    return order_result.status();
  }

  config.type = ck_scene_based;
  config.sbc.order = *order_result;
  return absl::OkStatus();
}

absl::Status FillChannelBasedConfig(
    const AudioElementObu::AudioElementConfig& audio_element_config,
    oar_audio_element_config_t& config) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&audio_element_config);
  if (channel_config == nullptr) {
    return absl::InvalidArgumentError(
        "Channel config is inconsistent with audio element type.");
  }
  if (channel_config->channel_audio_layer_configs.empty()) {
    return absl::InvalidArgumentError(
        "Channel config must have at least 1 layer.");
  }
  const auto& highest_channel_audio_layer_config =
      channel_config->channel_audio_layer_configs.back();
  const auto& layout_result = LookupOarLayoutFromScalableLoudspeakerLayout(
      highest_channel_audio_layer_config);
  if (!layout_result.ok()) {
    return layout_result.status();
  }

  config.type = ck_channel_based;
  config.cbc.layout = *layout_result;
  return absl::OkStatus();
}

absl::Status FillObjectBasedConfig(
    const AudioElementObu::AudioElementConfig& audio_element_config,
    oar_audio_element_config_t& config) {
  const auto* objects_config =
      std::get_if<ObjectsConfig>(&audio_element_config);
  if (objects_config == nullptr) {
    return absl::InvalidArgumentError(
        "Objects config is inconsistent with audio element type.");
  }

  config.type = ck_object_based;
  config.obc.num_objects = objects_config->GetNumObjects();
  return absl::OkStatus();
}

absl::Status FillAudioElementConfig(
    const AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& audio_element_config,
    oar_audio_element_config_t& config) {
  switch (audio_element_type) {
    case AudioElementObu::kAudioElementSceneBased:
      return FillSceneBasedConfig(audio_element_config, config);
    case AudioElementObu::kAudioElementChannelBased:
      return FillChannelBasedConfig(audio_element_config, config);
    case AudioElementObu::kAudioElementObjectBased:
      return FillObjectBasedConfig(audio_element_config, config);
    case AudioElementObu::kAudioElementBeginReserved:
    case AudioElementObu::kAudioElementEndReserved:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported audio element type= ", audio_element_type));
  }

  return absl::OkStatus();
}

absl::Status FillDemixingInfo(
    const DemixingParamDefinition& demixing_param_definition,
    const uint32_t audio_element_id, oar_audio_element_config_t& config,
    absl::flat_hash_map<uint32_t, uint32_t>& id_to_demixing_info_ids) {
  const auto& default_demixing_info =
      demixing_param_definition.default_demixing_info_parameter_data_;
  config.parameters.flags |= def_parameter_set_flag_iamf_downmix_info;
  config.parameters.downmix_info = {
      .mode = static_cast<int>(default_demixing_info.dmixp_mode),
      .weight_index = static_cast<int>(default_demixing_info.default_w),
  };

  const auto [iter, inserted] = id_to_demixing_info_ids.insert(
      {audio_element_id, demixing_param_definition.GetParameterId()});
  if (!inserted) {
    return absl::AlreadyExistsError(absl::StrCat(
        "Demixing info ID for Audio element ID= ", audio_element_id,
        " already exists."));
  }

  return absl::OkStatus();
}

absl::Status FillRenderingConfig(const RenderingConfig& rendering_config,
                                 oar_audio_element_config_t& config) {
  oar_headphones_rendering_mode_t headphones_rendering_mode;
  switch (rendering_config.headphones_rendering_mode) {
    using enum RenderingConfig::HeadphonesRenderingMode;
    case kHeadphonesRenderingModeStereo:
      headphones_rendering_mode = ck_world_locked_restricted;
      break;
    case kHeadphonesRenderingModeBinauralWorldLocked:
      headphones_rendering_mode = ck_world_locked;
      break;
    case kHeadphonesRenderingModeBinauralHeadLocked:
      headphones_rendering_mode = ck_head_locked;
      break;
    case kHeadphonesRenderingModeReserved3:
      headphones_rendering_mode = ck_reserved;
      break;
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported headphones rendering mode= ",
                       rendering_config.headphones_rendering_mode));
  }

  oar_binaural_filter_profile_t binaural_filter_profile =
      ck_binaural_filter_profile_default;
  switch (rendering_config.binaural_filter_profile) {
    using enum RenderingConfig::BinauralFilterProfile;
    case kBinauralFilterProfileAmbient:
      binaural_filter_profile = ck_ambient;
      break;
    case kBinauralFilterProfileDirect:
      binaural_filter_profile = ck_direct;
      break;
    case kBinauralFilterProfileReverberant:
      binaural_filter_profile = ck_reverberant;
      break;
    case kBinauralFilterProfileReserved3:
      // The Spec dictates that this shall be ignored. Keeping the default
      // value.
      break;
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported binaural filter profile= ",
                       rendering_config.binaural_filter_profile));
  }

  config.parameters.flags |=
      def_parameter_set_flag_iamf_element_rendering_config;
  config.parameters.element_rendering_config = {
      .headphones_rendering_mode = headphones_rendering_mode,
      .binaural_filter_profile = binaural_filter_profile,
  };

  return absl::OkStatus();
}

absl::Status FillMixGainMetadata(
    uint32_t audio_element_or_group_id, uint32_t parameter_id,
    uint32_t duration, float default_gain_value,
    const std::optional<MixGainParameterData>& mix_gain_parameter_data,
    oar_metadata_t& metadata) {
  metadata.type = ck_metadata_gain;

  // `liboar` uses a signed integer to represent duration. Cast here.
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, int>("duration", duration,
                                                      metadata.duration));

  oar_metadata_gain_t& gain = metadata.gain;
  gain.id = parameter_id;
  if (!mix_gain_parameter_data.has_value()) {
    gain.param_type = ck_param_constant;
    gain.constant_gain = default_gain_value;
    return absl::OkStatus();
  }

  gain.param_type = ck_param_animated;
  animated_float32_t& animated_gains = gain.animated_gains;
  const auto& param_data = mix_gain_parameter_data->param_data;
  switch (param_data.animation_type()) {
    using enum AnimationType;
    case kStep:
      animated_gains.animation_type = ck_animation_type_step;
      ABSL_DCHECK(param_data.start_point_value().has_value());
      animated_gains.data.start = Q7_8ToFloat(*param_data.start_point_value());
      break;
    case kLinear:
      animated_gains.animation_type = ck_animation_type_linear;
      ABSL_DCHECK(param_data.start_point_value().has_value());
      ABSL_DCHECK(param_data.end_point_value().has_value());
      animated_gains.data.start = Q7_8ToFloat(*param_data.start_point_value());
      animated_gains.data.end = Q7_8ToFloat(*param_data.end_point_value());
      break;
    case kBezier:
      animated_gains.animation_type = ck_animation_type_bezier;
      ABSL_DCHECK(param_data.start_point_value().has_value());
      ABSL_DCHECK(param_data.end_point_value().has_value());
      ABSL_DCHECK(param_data.control_point_value().has_value());
      ABSL_DCHECK(param_data.control_point_relative_time().has_value());
      animated_gains.data.start = Q7_8ToFloat(*param_data.start_point_value());
      animated_gains.data.end = Q7_8ToFloat(*param_data.end_point_value());
      animated_gains.data.control =
          Q7_8ToFloat(*param_data.control_point_value());
      animated_gains.data.control_relative_time =
          static_cast<float>(*param_data.control_point_relative_time()) /
          256.0f;
      break;
    // TODO(b/553298236): Support inter-linear and inter-bezier gains.
    case kInterLinear:
    case kInterBezier:
    default:
      return absl::UnimplementedError(absl::StrCat(
          "Unsupported animation type= ", param_data.animation_type()));
  }

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<OarWrapper> OarWrapper::Create(const Layout& output_layout,
                                               uint32_t sample_rate,
                                               uint32_t num_output_channels,
                                               uint32_t num_samples_per_frame,
                                               uint32_t output_mix_gain_id) {
  oar_layout_t target_layout;

  if (const auto layout_status = FillTargetLayout(output_layout, target_layout);
      !layout_status.ok()) {
    ABSL_LOG(ERROR) << layout_status;
    return nullptr;
  }

  // TODO(b/576563728): Update this check once the frame size limitations are
  //                    finalized.
  if (target_layout == ck_oar_layout_binaural &&
      (num_samples_per_frame > kBinauralMaxFrameSize ||
       num_samples_per_frame < kBinauralMinFrameSize)) {
    ABSL_LOG(ERROR) << "Rendering to binaural with frame size > "
                    << kBinauralMaxFrameSize << " or < "
                    << kBinauralMinFrameSize << " is not supported; got "
                    << num_samples_per_frame;
    return nullptr;
  }

  oar_config_t oar_config = {
      .target_layout = target_layout,
      .samples_per_channel = num_samples_per_frame,
      .sampling_rate = sample_rate,
  };
  std::unique_ptr<Oar, OarDeleter> oar(oar_create(&oar_config));
  if (oar == nullptr) {
    ABSL_LOG(ERROR) << "Failed to create an OAR instance.";
    return nullptr;
  }
  const uint32_t num_output_channels_created =
      oar_get_number_of_output_channels(oar.get());
  if (num_output_channels_created != num_output_channels) {
    ABSL_LOG(ERROR) << "Number of output channels requested != "
                    << " number of output channels created ("
                    << num_output_channels << " vs "
                    << num_output_channels_created << ").";
    return nullptr;
  }

  int add_group_result = oar_add_audio_group(oar.get());
  if (add_group_result < 0) {
    ABSL_LOG(ERROR) << "Failed to add an audio group. Error code= "
                    << add_group_result;
    return nullptr;
  }

  uint32_t group_id;
  auto cast_result = StaticCastIfInRange<int, uint32_t>(
      "group ID", add_group_result, group_id);
  if (!cast_result.ok()) {
    ABSL_LOG(ERROR) << "Fail to cast group ID. Status= " << cast_result;
    return nullptr;
  }

  return absl::WrapUnique(new OarWrapper(std::move(oar), num_samples_per_frame,
                                         group_id, output_mix_gain_id));
}

OarWrapper::OarWrapper(std::unique_ptr<Oar, OarDeleter> oar,
                       uint32_t num_samples_per_frame, uint32_t group_id,
                       uint32_t output_mix_gain_id)
    : oar_(std::move(oar)),
      num_output_channels_(oar_get_number_of_output_channels(oar_.get())),
      num_samples_per_frame_(num_samples_per_frame),
      group_id_(group_id),
      output_mix_gain_id_(output_mix_gain_id) {}

OarWrapper::~OarWrapper() {}

absl::Status OarWrapper::AddAudioElement(
    const uint32_t audio_element_id,
    const AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& audio_element_config,
    const std::optional<DemixingParamDefinition>& demixing_param_definition,
    const RenderingConfig& rendering_config, uint32_t element_mix_gain_id) {
  // Initialize and populate element config.
  oar_audio_element_config_t config = {};
  RETURN_IF_NOT_OK(
      FillAudioElementConfig(audio_element_type, audio_element_config, config));

  if (demixing_param_definition.has_value()) {
    RETURN_IF_NOT_OK(FillDemixingInfo(*demixing_param_definition,
                                      audio_element_id, config,
                                      id_to_demixing_info_ids_));
  }
  RETURN_IF_NOT_OK(FillRenderingConfig(rendering_config, config));

  const int add_result =
      oar_add_audio_element(oar_.get(), group_id_, audio_element_id, &config);
  if (add_result != kOarSuccess) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to add an audio element. Error code= ", add_result));
  }

  const auto [iter, inserted] = id_to_element_mix_gain_ids_.insert(
      {audio_element_id, element_mix_gain_id});
  if (!inserted) {
    return absl::AlreadyExistsError(absl::StrCat(
        "Element mix gain ID for audio element ID= ", audio_element_id,
        " already exists."));
  }

  return absl::OkStatus();
}

absl::Status OarWrapper::RemoveAudioElement(uint32_t id) {
  const int result = oar_remove_audio_element(oar_.get(), id);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to remove audio element ID= ", id, ". Error code= ", result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::AddInput(uint32_t audio_element_id,
                                  absl::Span<const OarSampleType> input_buffer,
                                  uint32_t num_channels) {
  if (!id_to_element_mix_gain_ids_.contains(audio_element_id)) {
    return absl::NotFoundError(
        absl::StrCat("Audio element ID= ", audio_element_id, " not found."));
  }

  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "input_buffer", input_buffer, num_samples_per_frame_ * num_channels));
  oar_audio_block_t input_data = {
      // `const_cast` is necessary because `liboar` takes a non-const pointer,
      // but we want to keep the C++ wrapper clear with the intention that this
      // function does not modify the input.
      .data = const_cast<OarSampleType*>(input_buffer.data()),
      .channels = num_channels,
      .samples_per_channel = num_samples_per_frame_,
  };

  const auto update_data_result =
      oar_update_audio_element_data(oar_.get(), audio_element_id, &input_data);
  if (update_data_result != kOarSuccess) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to update sample data for audio element ID= ", audio_element_id,
        ". Error code= ", update_data_result));
  }

  return absl::OkStatus();
}

absl::Status OarWrapper::UpdateElementMixGain(
    uint32_t audio_element_id, uint32_t duration, float default_gain_value,
    const std::optional<MixGainParameterData>& mix_gain_parameter_data) {
  auto element_mix_gain_id =
      LookupInMap(id_to_element_mix_gain_ids_, audio_element_id,
                  "Element mix gain ID for audio element ID");
  if (!element_mix_gain_id.ok()) {
    return element_mix_gain_id.status();
  }

  oar_metadata_t metadata;
  RETURN_IF_NOT_OK(FillMixGainMetadata(audio_element_id, *element_mix_gain_id,
                                       duration, default_gain_value,
                                       mix_gain_parameter_data, metadata));

  const auto update_metadata_result = oar_update_audio_element_metadata(
      oar_.get(), audio_element_id, &metadata);
  if (update_metadata_result != kOarSuccess) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to update element mix gain for audio element ID= ",
        audio_element_id, ". Error code= ", update_metadata_result));
  }

  return absl::OkStatus();
}

absl::Status OarWrapper::UpdateOutputMixGain(
    uint32_t duration, float default_gain_value,
    const std::optional<MixGainParameterData>& mix_gain_parameter_data) {
  oar_metadata_t metadata;
  RETURN_IF_NOT_OK(FillMixGainMetadata(group_id_, output_mix_gain_id_, duration,
                                       default_gain_value,
                                       mix_gain_parameter_data, metadata));

  const auto update_metadata_result =
      oar_update_metadata(oar_.get(), group_id_, &metadata);
  if (update_metadata_result != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to update output mix gain for audio group ID= ", group_id_,
        ". Error code= ", update_metadata_result));
  }

  return absl::OkStatus();
}

absl::Status OarWrapper::SetHeadRotation(float w, float x, float y, float z,
                                         uint32_t duration) {
  oar_metadata_t metadata;
  metadata.type = ck_metadata_head_rotation;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, int>("duration", duration,
                                                      metadata.duration));
  metadata.head_rotation.w = w;
  metadata.head_rotation.x = x;
  metadata.head_rotation.y = y;
  metadata.head_rotation.z = z;

  const int result = oar_update_metadata(oar_.get(), group_id_, &metadata);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to set head rotation. Error code= ", result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::UpdateObjectPositions(
    uint32_t audio_element_id, uint32_t duration,
    const std::vector<OarPositionData>& positions) {
  if (!id_to_element_mix_gain_ids_.contains(audio_element_id)) {
    return absl::NotFoundError(
        absl::StrCat("Audio element ID= ", audio_element_id, " not found."));
  }

  oar_metadata_t metadata;
  metadata.type = ck_metadata_object_positions;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, int>("duration", duration,
                                                      metadata.duration));

  // TODO(b/516667116): Support more than just constant positions.
  metadata.object_positions.param_type = ck_param_constant;
  metadata.object_positions.position_type = ck_polar;

  // Only `kMaxNumObjects` objects are supported for object-based
  // rendering in OAR.
  if (positions.size() > kMaxNumObjects) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Maximum number of object positions per element exceeded: (",
        positions.size(), " > ", kMaxNumObjects, ")."));
  }

  for (int i = 0; i < positions.size(); ++i) {
    metadata.object_positions.polar_positions[i].azimuth = positions[i].azimuth;
    metadata.object_positions.polar_positions[i].elevation =
        positions[i].elevation;
    metadata.object_positions.polar_positions[i].distance =
        positions[i].distance;
  }

  const auto update_metadata_result = oar_update_audio_element_metadata(
      oar_.get(), audio_element_id, &metadata);
  if (update_metadata_result != kOarSuccess) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to update object positions for audio element ID= ",
        audio_element_id, ". Error code= ", update_metadata_result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::UpdateDownMixMode(
    uint32_t audio_element_id, uint32_t duration,
    DemixingInfoParameterData::DMixPMode mode) {
  // `liboar` does not support the reserved modes.
  if (mode == DemixingInfoParameterData::kDMixPModeReserved1 ||
      mode == DemixingInfoParameterData::kDMixPModeReserved2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported reserved downmix mode= ", mode));
  }

  if (!id_to_demixing_info_ids_.contains(audio_element_id)) {
    return absl::NotFoundError(
        absl::StrCat("Demixing info for audio element ID= ", audio_element_id,
                     " not found."));
  }

  oar_metadata_t metadata;
  metadata.type = ck_metadata_iamf_downmix_mode;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, int>("duration", duration,
                                                      metadata.duration));

  metadata.iamf_downmix_mode.mode = static_cast<int>(mode);
  const int update_metadata_result = oar_update_audio_element_metadata(
      oar_.get(), audio_element_id, &metadata);
  if (update_metadata_result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to update downmix mode to mode= ", mode,
                     " for audio element ID= ", audio_element_id,
                     ". Error code= ", update_metadata_result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::Render(absl::Span<OarSampleType> output_buffer) {
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "output_buffer", output_buffer,
      num_output_channels_ * num_samples_per_frame_));
  oar_audio_block_t output_data = {
      .data = output_buffer.data(),
      .channels = num_output_channels_,
      .samples_per_channel = num_samples_per_frame_,
  };

  const auto render_result = oar_render(oar_.get(), &output_data);
  if (render_result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to render. Error code= ", render_result));
  }

  return absl::OkStatus();
}

absl::Status OarWrapper::EnableLoudnessProcessor(bool enable) {
  const int result = oar_enable_loudness_processor(oar_.get(), enable ? 1 : 0);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to ", (enable ? "enable" : "disable"),
                     " loudness processor. Error code= ", result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::SetLoudness(float loudness, float target_loudness) {
  const int result =
      oar_set_loudness(oar_.get(), group_id_, loudness, target_loudness);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to set loudness from ", loudness, " to ",
                     target_loudness, ". Error code= ", result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::EnableLimiter(bool enable) {
  const int result = oar_enable_limiter(oar_.get(), enable ? 1 : 0);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to ", (enable ? "enable" : "disable"),
                     " limiter. Error code= ", result));
  }
  return absl::OkStatus();
}

absl::Status OarWrapper::EnableHeadTracking(bool enable) {
  const int result = oar_enable_head_tracking(oar_.get(), enable ? 1 : 0);
  if (result != kOarSuccess) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to ", (enable ? "enable" : "disable"),
                     " head tracking. Error code= ", result));
  }
  return absl::OkStatus();
}

uint32_t OarWrapper::GetNumSamplesPerFrame() const {
  return oar_get_samples_per_channel(oar_.get());
}

uint32_t OarWrapper::GetSamplingRate() const {
  return oar_get_sampling_rate(oar_.get());
}

uint32_t OarWrapper::GetNumAudioElementChannels(uint32_t id) const {
  return oar_get_number_of_audio_element_channels(oar_.get(), id);
}

uint32_t OarWrapper::GetNumOutputChannels() const {
  return oar_get_number_of_output_channels(oar_.get());
}

uint32_t OarWrapper::GetNumAudioElements() const {
  return oar_get_number_of_audio_elements(oar_.get());
}

void OarWrapper::OarDeleter::operator()(Oar* oar) const { oar_destroy(oar); }

}  // namespace iamf_tools
