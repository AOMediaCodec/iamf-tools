/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/api/conversion/mix_presentation_conversion.h"

#include <variant>

#include "gtest/gtest.h"
#include "iamf/api/types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

TEST(ApiToInternalType_OutputLayout, ConvertsOutputStereoToInternalLayout) {
  Layout layout = ApiToInternalType(api::OutputLayout::kOutputStereo);
  EXPECT_EQ(layout.layout_type, Layout::kLayoutTypeLoudspeakersSsConvention);
  EXPECT_TRUE(std::holds_alternative<LoudspeakersSsConventionLayout>(
      layout.specific_layout));
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
                .sound_system,
            LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0);
}

}  // namespace
}  // namespace iamf_tools
