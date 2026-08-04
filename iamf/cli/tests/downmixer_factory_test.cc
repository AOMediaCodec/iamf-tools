/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/downmixer_factory.h"

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOkAndHolds;
using testing::IsEmpty;
using testing::SizeIs;
using enum ChannelLabel::Label;

constexpr DecodedUleb128 kStereoSubstreamId = 2;

TEST(CreateScalableChannelDownmixers, EmptyConfigIsOk) {
  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      /*labels_to_downmix=*/{}, /*substream_id_to_labels=*/{});

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(CreateScalableChannelDownmixers, HasOneDownMixerForTwoLayerStereo) {
  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      /*labels_to_downmix=*/{kL2, kR2},
      /*substream_id_to_labels=*/{{0, {kMono}}, {1, {kL2}}});

  EXPECT_THAT(down_mixers, IsOkAndHolds(SizeIs(1)));
}

TEST(CreateScalableChannelDownmixers,
     OneLayerChannelBasedHasNoDownMixersWithStereo) {
  const absl::flat_hash_set<ChannelLabel::Label> kStereoInputLabels = {kL2,
                                                                       kR2};
  const SubstreamIdLabelsMap kOneLayerStereoOutputIdToLabels = {
      {kStereoSubstreamId, {kL2, kR2}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      kStereoInputLabels, kOneLayerStereoOutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(CreateScalableChannelDownmixers,
     OneLayerChannelBasedHasNoDownMixersWith7_1_4) {
  const absl::flat_hash_set<ChannelLabel::Label> k7_1_4InputLabels = {
      kL7,   kR7,   kCentre, kLFE,  kLss7, kRss7,
      kLrs7, kRrs7, kLtf4,   kRtf4, kLtb4, kRtb4};
  const SubstreamIdLabelsMap kOneLayer7_1_4OutputIdToLabels = {
      {0, {kL7, kR7}},     {1, {kLss7, kRss7}}, {2, {kLrs7, kRrs7}},
      {3, {kLtf4, kRtf4}}, {4, {kLtb4, kRtb4}}, {5, {kCentre}},
      {6, {kLFE}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      k7_1_4InputLabels, kOneLayer7_1_4OutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

// TODO(b/450899154): Add ambisonics down mixers.
TEST(CreateScalableChannelDownmixers, AmbisonicsHasNoDownMixers) {
  const absl::flat_hash_set<ChannelLabel::Label> kAmbisonicsInputLabels = {
      kA0, kA1, kA2, kA3};
  const SubstreamIdLabelsMap kAmbisonicsOutputIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      kAmbisonicsInputLabels, kAmbisonicsOutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

}  // namespace
}  // namespace iamf_tools
