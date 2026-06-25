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

#include "iamf/cli/labeled_frame.h"

#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using enum ChannelLabel::Label;
using ::testing::DoubleEq;
using ::testing::Pointwise;

TEST(FindSamplesOrDemixedSamples, FindsMatchingSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap label_to_samples = {{kL2, kSamplesToFind}};

  absl::Span<const InternalSampleType> samples;
  EXPECT_THAT(FindSamplesOrDemixedSamples(kL2, label_to_samples, samples),
              IsOk());
  EXPECT_THAT(samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, FindsMatchingDemixedSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap label_to_samples = {{kDemixedR2, kSamplesToFind}};

  absl::Span<const InternalSampleType> samples;
  EXPECT_THAT(FindSamplesOrDemixedSamples(kR2, label_to_samples, samples),
              IsOk());
  EXPECT_THAT(samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, InvalidWhenThereIsNoDemixingLabel) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap label_to_samples = {{kDemixedR2, kSamplesToFind}};

  absl::Span<const InternalSampleType> samples;
  EXPECT_THAT(FindSamplesOrDemixedSamples(kL2, label_to_samples, samples),
              Not(IsOk()));
}

TEST(FindSamplesOrDemixedSamples, RegularSamplesTakePrecedence) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const std::vector<InternalSampleType> kDemixedSamplesToIgnore = {4, 5, 6};
  const LabelSamplesMap label_to_samples = {
      {kR2, kSamplesToFind}, {kDemixedR2, kDemixedSamplesToIgnore}};

  absl::Span<const InternalSampleType> samples;
  EXPECT_THAT(FindSamplesOrDemixedSamples(kR2, label_to_samples, samples),
              IsOk());
  EXPECT_THAT(samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, ErrorNoMatchingSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap label_to_samples = {{kL2, kSamplesToFind}};

  absl::Span<const InternalSampleType> samples;
  EXPECT_THAT(FindSamplesOrDemixedSamples(kL3, label_to_samples, samples),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
