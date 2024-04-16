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
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"

namespace iamf_tools {
namespace {

TEST(ArrangeSamplesToRender, SucceedsOnEmptyFrame) {
  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(
      AudioElementRendererBase::ArrangeSamplesToRender({}, {}, samples).ok());
  EXPECT_TRUE(samples.empty());
}

TEST(ArrangeSamplesToRender, ArrangesSamplesInTimeChannelAxes) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{"L2", {0, 1, 2}}, {"R2", {10, 11, 12}}}};
  const std::vector<std::string> kStereoArrangement = {"L2", "R2"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kStereoLabeledFrame, kStereoArrangement, samples)
                  .ok());

  EXPECT_EQ(samples,
            std::vector<std::vector<int32_t>>({{0, 10}, {1, 11}, {2, 12}}));
}

TEST(ArrangeSamplesToRender, FindsDemixedLabels) {
  const LabeledFrame kDemixedTwoLayerStereoFrame = {
      .label_to_samples = {{"M", {75}}, {"L2", {50}}, {"D_R2", {100}}}};
  const std::vector<std::string> kStereoArrangement = {"L2", "R2"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kDemixedTwoLayerStereoFrame, kStereoArrangement, samples)
                  .ok());

  EXPECT_EQ(samples, std::vector<std::vector<int32_t>>({{50, 100}}));
}

TEST(ArrangeSamplesToRender, IgnoresExtraLabels) {
  const LabeledFrame kStereoLabeledFrameWithExtraLabel = {
      .label_to_samples = {{"L2", {0}}, {"R2", {10}}, {"LFE", {999}}}};
  const std::vector<std::string> kStereoArrangement = {"L2", "R2"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(
      AudioElementRendererBase::ArrangeSamplesToRender(
          kStereoLabeledFrameWithExtraLabel, kStereoArrangement, samples)
          .ok());
  EXPECT_EQ(samples, std::vector<std::vector<int32_t>>({{0, 10}}));
}

TEST(ArrangeSamplesToRender, LeavesEmptyLabelsZero) {
  const LabeledFrame kMixedFirstOrderAmbisonicsFrame = {
      .label_to_samples = {
          {"A0", {1, 2}}, {"A2", {201, 202}}, {"A3", {301, 302}}}};
  const std::vector<std::string> kMixedFirstOrderAmbisonicsArrangement = {
      "A0", "", "A2", "A3"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kMixedFirstOrderAmbisonicsFrame,
                  kMixedFirstOrderAmbisonicsArrangement, samples)
                  .ok());
  EXPECT_EQ(samples, std::vector<std::vector<int32_t>>(
                         {{1, 0, 201, 301}, {2, 0, 202, 302}}));
}

TEST(ArrangeSamplesToRender, ExcludesSamplesToBeTrimmed) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 2,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{"M", {999, 100, 999, 999}}}};
  const std::vector<std::string> kMonoArrangement = {"M"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kMonoLabeledFrameWithSamplesToTrim, kMonoArrangement, samples)
                  .ok());
  EXPECT_EQ(samples, std::vector<std::vector<int32_t>>({{100}}));
}

TEST(ArrangeSamplesToRender, ClearsInputVector) {
  const LabeledFrame kMonoLabeledFrame = {.label_to_samples = {{"M", {1, 2}}}};
  const std::vector<std::string> kMonoArrangement = {"M"};

  std::vector<std::vector<int32_t>> samples = {{999, 999}};
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kMonoLabeledFrame, kMonoArrangement, samples)
                  .ok());
  EXPECT_EQ(samples, std::vector<std::vector<int32_t>>({{1}, {2}}));
}

TEST(ArrangeSamplesToRender, TrimmingAllFramesFromStartIsResultsInEmptyOutput) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 4,
      .label_to_samples = {{"M", {999, 999, 999, 999}}}};
  const std::vector<std::string> kMonoArrangement = {"M"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_TRUE(AudioElementRendererBase::ArrangeSamplesToRender(
                  kMonoLabeledFrameWithSamplesToTrim, kMonoArrangement, samples)
                  .ok());
  EXPECT_TRUE(samples.empty());
}

TEST(ArrangeSamplesToRender,
     InvalidWhenRequestedLabelsHaveDifferentNumberOfSamples) {
  const LabeledFrame kStereoLabeledFrameWithMissingSample = {
      .label_to_samples = {{"L2", {0, 1}}, {"R2", {10}}}};
  const std::vector<std::string> kStereoArrangement = {"L2", "R2"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_FALSE(
      AudioElementRendererBase::ArrangeSamplesToRender(
          kStereoLabeledFrameWithMissingSample, kStereoArrangement, samples)
          .ok());
}

TEST(ArrangeSamplesToRender, InvalidWhenTrimIsImplausible) {
  const LabeledFrame kFrameWithExcessSamplesTrimmed = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 2,
      .label_to_samples = {{"L2", {0, 1}}, {"R2", {10, 11}}}};
  const std::vector<std::string> kStereoArrangement = {"L2", "R2"};

  std::vector<std::vector<int32_t>> samples;
  EXPECT_FALSE(AudioElementRendererBase::ArrangeSamplesToRender(
                   kFrameWithExcessSamplesTrimmed, kStereoArrangement, samples)
                   .ok());
}

TEST(ArrangeSamplesToRender, InvalidMissingLabel) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{"L2", {0}}, {"R2", {10}}}};
  const std::vector<std::string> kMonoArrangement = {"M"};

  std::vector<std::vector<int32_t>> unused_samples;
  EXPECT_FALSE(AudioElementRendererBase::ArrangeSamplesToRender(
                   kStereoLabeledFrame, kMonoArrangement, unused_samples)
                   .ok());
}

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
