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
#include "iamf/cli/descriptor_obus.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::testing::IsEmpty;
using ::testing::Pointee;

TEST(DescriptorObusConstructor, CodecConfigObusIsValidAndEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_THAT(descriptor_obus.codec_config_obus, Pointee(IsEmpty()));
}

TEST(DescriptorObusConstructor, AudioElementsIsValidAndEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_THAT(descriptor_obus.audio_elements, Pointee(IsEmpty()));
}

TEST(DescriptorObusConstructor, MixPresentationObusIsEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_TRUE(descriptor_obus.mix_presentation_obus.empty());
}

}  // namespace
}  // namespace iamf_tools
