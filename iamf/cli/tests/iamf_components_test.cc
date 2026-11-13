/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/iamf_components.h"

#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/leb_generator.h"

namespace iamf_tools {
namespace {

TEST(IamfComponentsTest, CreateLoudnessCalculatorFactoryReturnsNonNull) {
  EXPECT_NE(CreateLoudnessCalculatorFactory(), nullptr);
}

TEST(IamfComponentsTest,
     CreateObuSequencersReturnsNonNullAndNonZeroObuSequencers) {
  const auto leb_generator = LebGenerator::Create();
  ASSERT_NE(leb_generator, nullptr);

  auto obu_sequencers = CreateObuSequencers(
      "", GetAndCreateOutputDirectory("iamf_directory"), false, *leb_generator);

  EXPECT_FALSE(obu_sequencers.empty());
  for (auto& obu_sequencer : obu_sequencers) {
    EXPECT_NE(obu_sequencer, nullptr);
  }
}

TEST(IamfComponentsTest, CanBeConfiguredWithFixedSizeLebGenerator) {
  const auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);
  ASSERT_NE(leb_generator, nullptr);

  auto obu_sequencers = CreateObuSequencers(
      "my_prefix", GetAndCreateOutputDirectory("iamf_directory"), false,
      *leb_generator);

  EXPECT_FALSE(obu_sequencers.empty());
  for (auto& obu_sequencer : obu_sequencers) {
    EXPECT_NE(obu_sequencer, nullptr);
  }
}

}  // namespace
}  // namespace iamf_tools
