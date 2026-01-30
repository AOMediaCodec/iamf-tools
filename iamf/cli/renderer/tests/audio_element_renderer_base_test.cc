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

#include <cstddef>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Each;
using ::testing::IsEmpty;

constexpr size_t kFourSamplesPerFrame = 4;
constexpr size_t kOneChannel = 1;

std::vector<std::vector<InternalSampleType>> GetSamplesToRender() {
  static const std::vector<std::vector<InternalSampleType>> kSamplesToRender = {
      {0.0, 0.1, 0.2, 0.3}};
  return kSamplesToRender;
}

// Mock renderer which "renders" `kSamplesToRender` each call to
// `RenderLabeledFrame`.
class MockAudioElementRenderer : public AudioElementRendererBase {
 public:
  MockAudioElementRenderer()
      : AudioElementRendererBase(
            /*ordered_labels=*/{}, kFourSamplesPerFrame, kOneChannel) {};

  absl::Status RenderSamples(
      absl::Span<const absl::Span<const InternalSampleType>>)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) override {
    rendered_samples_ = GetSamplesToRender();
    // UpdateRenderedSamplesSpans();
    return absl::OkStatus();
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

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  renderer.Flush(rendered_samples);

  EXPECT_EQ(rendered_samples.size(), kOneChannel);
  EXPECT_THAT(rendered_samples, Each(IsEmpty()));
}

TEST(AudioElementRendererBase, FlushingTwiceDoesNotAppendMore) {
  MockAudioElementRenderer renderer;
  std::vector<std::vector<InternalSampleType>>
      vector_to_collect_rendered_samples;

  EXPECT_THAT(renderer.RenderLabeledFrame({}), IsOk());
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());

  renderer.Flush(vector_to_collect_rendered_samples);
  EXPECT_FALSE(vector_to_collect_rendered_samples.empty());

  // Samples are already flushed. Flushing again is OK, but it does nothing.
  vector_to_collect_rendered_samples.clear();
  renderer.Flush(vector_to_collect_rendered_samples);
  for (const auto& rendered_samples_for_channel :
       vector_to_collect_rendered_samples) {
    EXPECT_TRUE(rendered_samples_for_channel.empty());
  }
}

TEST(AudioElementRendererBase, AppendsWhenFlushing) {
  MockAudioElementRenderer renderer;
  std::vector<std::vector<InternalSampleType>>
      vector_to_collect_rendered_samples = {{100, 200, 300, 400}};
  // Flush should append `kSamplesToRender` to the initial vector.
  auto expected_samples = vector_to_collect_rendered_samples;
  const auto kSamplesToRender = GetSamplesToRender();
  expected_samples[0].insert(expected_samples[0].end(),
                             kSamplesToRender[0].begin(),
                             kSamplesToRender[0].end());

  EXPECT_THAT(renderer.RenderLabeledFrame({}), IsOk());
  EXPECT_THAT(renderer.Finalize(), IsOk());
  EXPECT_TRUE(renderer.IsFinalized());

  renderer.Flush(vector_to_collect_rendered_samples);
  EXPECT_THAT(vector_to_collect_rendered_samples,
              InternalSamples2DMatch(expected_samples));
}

}  // namespace
}  // namespace iamf_tools
