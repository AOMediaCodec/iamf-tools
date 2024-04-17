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

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/precomputed_gains.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
/*\!brief Renders demixed channels to the requested output layout.
 *
 * This class represents a renderer which is suitable for use when the
 * associated audio element has a layer which does not matches the playback
 * layout according to IAMF Spec 7.3.2.1
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
class AudioElementRendererChannelToChannel : public AudioElementRendererBase {
 public:
  /*\!brief Creates a channel to channel renderer from a channel-based config.
   *
   * \param scalable_channel_layout_config Config for the scalable channel
   *     layout.
   * \param playback_layout Layout of the audio element to be rendered.
   * \return Render to use or `nullptr` on failure.
   */
  static std::unique_ptr<AudioElementRendererChannelToChannel>
  CreateFromScalableChannelLayoutConfig(
      const ScalableChannelLayoutConfig& scalable_channel_layout_config,
      const Layout& playback_layout);

  /*\!brief Destructor. */
  ~AudioElementRendererChannelToChannel() override = default;

  /*\!brief Accumulates samples to be rendered.
   *
   * \param labeled_frame Labeled frame to render.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RenderLabeledFrame(const LabeledFrame& labeled_frame) override;

 private:
  /*\!brief Constructor. */
  AudioElementRendererChannelToChannel(
      const std::vector<std::vector<double>>& gains)
      : gains_(gains) {}

  const std::vector<std::vector<double>> gains_;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
