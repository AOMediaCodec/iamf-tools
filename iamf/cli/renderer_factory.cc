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
#include "iamf/cli/renderer_factory.h"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/audio_element_renderer_channel_to_channel.h"
#include "iamf/cli/renderer/audio_element_renderer_passthrough.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

bool IsAudioElementRenderedBinaural(
    RenderingConfig::HeadphonesRenderingMode headphones_rendering_mode,
    Layout::LayoutType layout_type) {
  {
    return headphones_rendering_mode ==
               RenderingConfig::kHeadphonesRenderingModeBinaural &&
           layout_type == Layout::kLayoutTypeBinaural;
  }
}

std::unique_ptr<AudioElementRendererBase> MaybeCreateAmbisonicsRenderer(
    bool use_binaural, const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const AudioElementObu::AudioElementConfig& config,
    const Layout& loudness_layout, size_t num_samples_per_frame,
    size_t sample_rate) {
  const auto* ambisonics_config = std::get_if<AmbisonicsConfig>(&config);
  if (ambisonics_config == nullptr) {
    ABSL_LOG(ERROR)
        << "Ambisonics config is inconsistent with audio element type.";
    return nullptr;
  }

  if (use_binaural) {
    // TODO(b/450473100): Render ambisonics to binaural using OBR here.
    ABSL_LOG(WARNING)
        << "Skipping creating an Ambisonics to binaural-based "
           "renderer. Binaural rendering is not yet supported for "
           "ambisonics.";
    return nullptr;
  }

  return AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
      *ambisonics_config, audio_substream_ids, substream_id_to_labels,
      loudness_layout, num_samples_per_frame);
}

std::unique_ptr<AudioElementRendererBase> MaybeCreateChannelRenderer(
    bool use_binaural, const AudioElementObu::AudioElementConfig& config,
    const Layout& loudness_layout, size_t num_samples_per_frame,
    size_t sample_rate) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&config);
  if (channel_config == nullptr) {
    ABSL_LOG(ERROR)
        << "Channel config is inconsistent with audio element type.";
    return nullptr;
  }
  // Lazily try to make a pass-through renderer.
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          *channel_config, loudness_layout, num_samples_per_frame);
  if (pass_through_renderer != nullptr) {
    return pass_through_renderer;
  }

  if (use_binaural) {
    // TODO(b/450472803): Render channel layouts to binaural using OBR here.
    ABSL_LOG(WARNING)
        << "Skipping creating a channel to binaural-based renderer.";
    return nullptr;
  }
  return AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(*channel_config, loudness_layout,
                                            num_samples_per_frame);
}

}  // namespace

RendererFactoryBase::~RendererFactoryBase() {}

std::unique_ptr<AudioElementRendererBase>
RendererFactory::CreateRendererForLayout(
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& audio_element_config,
    const RenderingConfig& rendering_config, const Layout& loudness_layout,
    size_t num_samples_per_frame, size_t sample_rate) const {
  const bool use_binaural = IsAudioElementRenderedBinaural(
      rendering_config.headphones_rendering_mode, loudness_layout.layout_type);

  switch (audio_element_type) {
    case AudioElementObu::kAudioElementSceneBased:
      return MaybeCreateAmbisonicsRenderer(
          use_binaural, audio_substream_ids, substream_id_to_labels,
          audio_element_config, loudness_layout, num_samples_per_frame,
          sample_rate);
    case AudioElementObu::kAudioElementChannelBased:
      return MaybeCreateChannelRenderer(use_binaural, audio_element_config,
                                        loudness_layout, num_samples_per_frame,
                                        sample_rate);
    case AudioElementObu::kAudioElementBeginReserved:
    case AudioElementObu::kAudioElementEndReserved:
      ABSL_LOG(WARNING) << "Unsupported audio_element_type_= "
                        << audio_element_type;
      return nullptr;
  }
  // The above switch is exhaustive.
  ABSL_LOG(FATAL) << "Unsupported audio_element_type_= " << audio_element_type;
}

}  // namespace iamf_tools
