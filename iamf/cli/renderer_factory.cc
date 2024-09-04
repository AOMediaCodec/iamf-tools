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
#include <vector>

#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

RendererFactoryBase::~RendererFactoryBase() {}

std::unique_ptr<AudioElementRendererBase>
RendererFactory::CreateRendererForLayout(
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& config,
    const Layout& loudness_layout) const {
  // TODO(b/332567539): Implement and return renderers depending on the input
  //                    audio element and output layout.

  return nullptr;
}

}  // namespace iamf_tools
