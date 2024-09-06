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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief Passthrough demixed channels corresponding with output layout.
 *
 * This class represents a renderer which is suitable for use when the
 * associated audio element has a layer which matches the playback layout
 * according to IAMF Spec 7.3.2.1
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering-m2l).
 *
 * - Call `RenderAudioFrame()` to render a labeled frame. The rendering may
 *   happen asynchronously.
 * - Call `SamplesAvailable()` to see if there are samples available.
 * - Call `Flush()` to retrieve finished frames, in the order they were
 *   received by `RenderLabeledFrame()`.
 * - Call `Finalize()` to close the renderer, telling it to finish rendering
 *   any remaining frames, which can be retrieved one last time via `Flush()`.
 *   After calling `Finalize()`, any subsequent call to `RenderAudioFrame()`
 *   may fail.
 */
class AudioElementRendererPassThrough : public AudioElementRendererBase {
 public:
  /*!\brief Creates a passthrough renderer from a channel-based config.
   *
   * Creates a passthrough renderer if it is suitable for use according to IAMF
   * Spec 7.3.2.1. In particular when either of these conditions are true:
   *  - "If num_layers = 1, use the loudspeaker_layout of the Audio Element."
   *  - "Else, if there is an Audio Element with a loudspeaker_layout that
   * matches the playback layout, use it."
   *
   * \param scalable_channel_layout_config Config for the scalable channel
   *     layout.
   * \param playback_layout Layout of the audio element to be rendered.
   * \return Render to use or `nullptr` if it would not be suitable for use.
   */
  static std::unique_ptr<AudioElementRendererPassThrough>
  CreateFromScalableChannelLayoutConfig(
      const ScalableChannelLayoutConfig& scalable_channel_layout_config,
      const Layout& playback_layout);

  /*!\brief Destructor. */
  ~AudioElementRendererPassThrough() override = default;

 private:
  /*!\brief Constructor.
   *
   * \param ordered_labels Ordered list of channel labels to render.
   */
  AudioElementRendererPassThrough(
      const std::vector<ChannelLabel::Label>& ordered_labels)
      : AudioElementRendererBase(
            ordered_labels,
            // For a passthrough renderer, (number of output channels)
            // is the same as (number of input channels).
            /*num_output_channels=*/ordered_labels.size()) {}

  /*!\brief Renders samples.
   *
   * \param samples_to_render Samples to render arranged in (time, channel).
   * \param rendered_samples Output rendered samples.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RenderSamples(
      const std::vector<std::vector<InternalSampleType>>& samples_to_render,
      std::vector<InternalSampleType>& rendered_samples) override;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
