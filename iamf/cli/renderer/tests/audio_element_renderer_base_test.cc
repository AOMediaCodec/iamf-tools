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

#include "iamf/cli/renderer/audio_element_renderer_base.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"

namespace iamf_tools {
namespace {

const std::vector<int32_t> kSamplesToRender = {0, 1, 2, 3};

// Mock renderer which "renders" `kSamplesToRender` each call to
// `RenderLabeledFrame`.
class MockAudioElementRenderer : public AudioElementRendererBase {
 public:
  MockAudioElementRenderer() = default;

  absl::Status RenderLabeledFrame(const LabeledFrame& labeled_frame) override {
    absl::MutexLock lock(&mutex_);
    rendered_samples_.insert(rendered_samples_.end(), kSamplesToRender.begin(),
                             kSamplesToRender.end());
    return absl::OkStatus();
  }
};

TEST(AudioElementRendererBase, FinalizeAndFlushWithOutRenderingSucceeds) {
  MockAudioElementRenderer renderer;
  EXPECT_TRUE(renderer.Finalize().ok());
  std::vector<int32_t> rendered_samples;
  EXPECT_TRUE(renderer.Flush(rendered_samples).ok());
  EXPECT_TRUE(rendered_samples.empty());
}

TEST(AudioElementRendererBase, FlushingTwiceDoesNotAppendMore) {
  MockAudioElementRenderer renderer;
  std::vector<int32_t> vector_to_collect_rendered_samples;

  EXPECT_TRUE(renderer.RenderLabeledFrame({}).ok());
  EXPECT_TRUE(renderer.Finalize().ok());
  EXPECT_TRUE(renderer.Flush(vector_to_collect_rendered_samples).ok());
  EXPECT_FALSE(vector_to_collect_rendered_samples.empty());
  vector_to_collect_rendered_samples.clear();

  // Samples are already flushed. Flushing again is OK, but it does nothing.
  EXPECT_TRUE(renderer.Flush(vector_to_collect_rendered_samples).ok());
  EXPECT_TRUE(vector_to_collect_rendered_samples.empty());
}

TEST(AudioElementRendererBase, AppendsWhenFlushing) {
  MockAudioElementRenderer renderer;
  std::vector<int32_t> vector_to_collect_rendered_samples({100, 200, 300, 400});
  // Flush should append `kSamplesToRender` to the initial vector.
  std::vector<int32_t> expected_samples(vector_to_collect_rendered_samples);
  expected_samples.insert(expected_samples.end(), kSamplesToRender.begin(),
                          kSamplesToRender.end());

  EXPECT_TRUE(renderer.RenderLabeledFrame({}).ok());
  EXPECT_TRUE(renderer.Finalize().ok());
  EXPECT_TRUE(renderer.Flush(vector_to_collect_rendered_samples).ok());
  EXPECT_EQ(vector_to_collect_rendered_samples, expected_samples);
}

}  // namespace
}  // namespace iamf_tools
