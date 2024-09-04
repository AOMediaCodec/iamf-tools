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

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using testing::DoubleEq;
using testing::Pointwise;

const std::vector<int32_t> kSamplesToRender = {0, 1, 2, 3};

// Mock renderer which "renders" `kSamplesToRender` each call to
// `RenderLabeledFrame`.
class MockAudioElementRenderer : public AudioElementRendererBase {
 public:
  MockAudioElementRenderer() = default;

  absl::StatusOr<int> RenderLabeledFrame(
      const LabeledFrame& labeled_frame) override {
    absl::MutexLock lock(&mutex_);
    rendered_samples_.insert(rendered_samples_.end(), kSamplesToRender.begin(),
                             kSamplesToRender.end());
    return kSamplesToRender.size();
  }
};

TEST(AudioElementRendererBase, IsFinalizedReturnsFalseBeforeFinalizeIsCalled) {
  MockAudioElementRenderer renderer;
  EXPECT_FALSE(renderer.IsFinalized());
}

TEST(AudioElementRendererBase, BaseImmediatelyAfterFinalizeIsFinalized) {
  MockAudioElementRenderer renderer;
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());
}

TEST(AudioElementRendererBase, FinalizeAndFlushWithOutRenderingSucceeds) {
  MockAudioElementRenderer renderer;
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());
  std::vector<InternalSampleType> rendered_samples;
  EXPECT_THAT(renderer.Flush(rendered_samples), IsOk());
  EXPECT_TRUE(rendered_samples.empty());
}

TEST(AudioElementRendererBase, FlushingTwiceDoesNotAppendMore) {
  MockAudioElementRenderer renderer;
  std::vector<InternalSampleType> vector_to_collect_rendered_samples;

  EXPECT_THAT(renderer.RenderLabeledFrame({}), IsOk());
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());

  EXPECT_THAT(renderer.Flush(vector_to_collect_rendered_samples), IsOk());
  EXPECT_FALSE(vector_to_collect_rendered_samples.empty());
  vector_to_collect_rendered_samples.clear();

  // Samples are already flushed. Flushing again is OK, but it does nothing.
  EXPECT_THAT(renderer.Flush(vector_to_collect_rendered_samples), IsOk());
  EXPECT_TRUE(vector_to_collect_rendered_samples.empty());
}

TEST(AudioElementRendererBase, AppendsWhenFlushing) {
  MockAudioElementRenderer renderer;
  std::vector<InternalSampleType> vector_to_collect_rendered_samples(
      {100, 200, 300, 400});
  // Flush should append `kSamplesToRender` to the initial vector.
  std::vector<InternalSampleType> expected_samples(
      vector_to_collect_rendered_samples);
  expected_samples.insert(expected_samples.end(), kSamplesToRender.begin(),
                          kSamplesToRender.end());

  EXPECT_THAT(renderer.RenderLabeledFrame({}), IsOk());
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());

  EXPECT_THAT(renderer.Flush(vector_to_collect_rendered_samples), IsOk());
  EXPECT_THAT(vector_to_collect_rendered_samples,
              Pointwise(DoubleEq(), expected_samples));
}

}  // namespace
}  // namespace iamf_tools
