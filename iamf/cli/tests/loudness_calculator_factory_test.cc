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
#include "iamf/cli/loudness_calculator_factory.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

constexpr int32_t kUnusedRenderedSampleRate = 0;
constexpr int32_t kUnusedRenderedBitDepth = 0;

TEST(CreatePrimaryLoudnessCalculator, NeverReturnsNull) {
  const LoudnessCalculatorFactoryUserProvidedLoudness factory;
  const MixPresentationLayout layout = {};

  EXPECT_NE(factory.CreateLoudnessCalculator(layout, kUnusedRenderedSampleRate,
                                             kUnusedRenderedBitDepth),
            nullptr);
}

}  // namespace
}  // namespace iamf_tools
