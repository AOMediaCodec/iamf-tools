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

#include <memory>
#include <variant>
#include <vector>

#include "absl/log/log.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
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
    bool use_binaural, const AudioElementObu::AudioElementConfig& config) {
  const auto* ambisonics_config = std::get_if<AmbisonicsConfig>(&config);
  if (ambisonics_config == nullptr) {
    LOG(ERROR) << "Ambisonics config is inconsistent with audio element type.";
    return nullptr;
  }
  // TODO(b/332567539): Create ambisonics to channel and binaural based
  //                    renderers.
  LOG(WARNING) << "Skipping creating an Ambisonics to "
               << (use_binaural ? "binaural" : "channel") << "-based renderer.";
  return nullptr;
}

std::unique_ptr<AudioElementRendererBase> MaybeCreateChannelRenderer(
    bool use_binaural, const AudioElementObu::AudioElementConfig& config,
    const Layout& loudness_layout) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&config);
  if (channel_config == nullptr) {
    LOG(ERROR) << "Channel config is inconsistent with audio element type.";
    return nullptr;
  }
  // Lazily try to make a pass-through renderer.
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          *channel_config, loudness_layout);
  if (pass_through_renderer != nullptr) {
    return pass_through_renderer;
  }
  // TODO(b/332567539): Create channel to channel or binaural based
  //                    renderers .
  LOG(WARNING) << "Skipping creating an Ambisonics to "
               << (use_binaural ? "binaural" : "channel") << "-based renderer.";
  return nullptr;
}

}  // namespace

RendererFactoryBase::~RendererFactoryBase() {}

std::unique_ptr<AudioElementRendererBase>
RendererFactory::CreateRendererForLayout(
    const std::vector<DecodedUleb128>& /*audio_substream_ids*/,
    const SubstreamIdLabelsMap& /*substream_id_to_labels*/,
    AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& audio_element_config,
    const RenderingConfig& rendering_config,
    const Layout& loudness_layout) const {
  const bool use_binaural = IsAudioElementRenderedBinaural(
      rendering_config.headphones_rendering_mode, loudness_layout.layout_type);

  switch (audio_element_type) {
    case AudioElementObu::kAudioElementSceneBased:
      return MaybeCreateAmbisonicsRenderer(use_binaural, audio_element_config);
    case AudioElementObu::kAudioElementChannelBased:
      return MaybeCreateChannelRenderer(use_binaural, audio_element_config,
                                        loudness_layout);
    case AudioElementObu::kAudioElementBeginReserved:
    case AudioElementObu::kAudioElementEndReserved:
      LOG(WARNING) << "Unsupported audio_element_type_= " << audio_element_type;
      return nullptr;
  }
  // The above switch is exhaustive.
  LOG(FATAL) << "Unsupported audio_element_type_= " << audio_element_type;
}

}  // namespace iamf_tools
