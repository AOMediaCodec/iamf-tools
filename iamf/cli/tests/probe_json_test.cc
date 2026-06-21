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
#include "iamf/cli/probe_json.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/probe.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::HasSubstr;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr DecodedUleb128 kAudioElementId = 2;
constexpr DecodedUleb128 kSubstreamId = 18;
constexpr DecodedUleb128 kMixPresentationId = 3;
constexpr DecodedUleb128 kParameterId = 999;
constexpr DecodedUleb128 kParameterRate = 48000;

ProbeReport ProbeMinimalIaSequence(ProbeOptions options = {}) {
  const IASequenceHeaderObu sequence_header(ObuHeader(),
                                            ProfileVersion::kIamfSimpleProfile,
                                            ProfileVersion::kIamfBaseProfile);
  DescriptorObus::CodecConfigsById codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_configs);
  DescriptorObus::AudioElementsById audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                {kSubstreamId}, codec_configs,
                                                audio_elements);
  DescriptorObus::MixPresentationObus mix_presentations;
  AddMixPresentationObuWithAudioElementIds(kMixPresentationId,
                                           {kAudioElementId}, kParameterId,
                                           kParameterRate, mix_presentations);

  std::list<const ObuBase*> obus = {&sequence_header};
  for (const auto& [_, obu] : codec_configs) obus.push_back(&obu);
  for (const auto& [_, element] : audio_elements) obus.push_back(&element.obu);
  for (const auto& mp : mix_presentations) obus.push_back(&mp);
  auto data = SerializeObusExpectOk(obus);
  const AudioFrameObu audio_frame(ObuHeader(), kSubstreamId,
                                  /*audio_frame=*/{1, 2, 3, 4});
  const auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  data.insert(data.end(), temporal_unit.begin(), temporal_unit.end());

  auto report = Probe(absl::MakeConstSpan(data), options);
  EXPECT_THAT(report, IsOk());
  return *report;
}

TEST(ProbeReportToJson, EmitsTopLevelObjectWithDescriptorFields) {
  const std::string json = ProbeReportToJson(ProbeMinimalIaSequence());

  EXPECT_THAT(json, ::testing::StartsWith("{\n"));
  EXPECT_THAT(json, ::testing::EndsWith("}\n"));
  EXPECT_THAT(json, HasSubstr("\"primary_profile\": \"simple\""));
  EXPECT_THAT(json, HasSubstr("\"primary_profile_raw\": 0"));
  EXPECT_THAT(json, HasSubstr("\"additional_profile\": \"base\""));
  EXPECT_THAT(json, HasSubstr("\"codec_id\": \"Opus\""));
  EXPECT_THAT(json, HasSubstr("\"audio_elements\": ["));
  EXPECT_THAT(json, HasSubstr("\"mix_presentations\": ["));
  // No scan was requested, so the scan key is absent.
  EXPECT_THAT(json, ::testing::Not(HasSubstr("\"temporal_unit_scan\"")));
}

TEST(ProbeReportToJson, EmitsTemporalUnitScanWhenPresent) {
  ProbeOptions options;
  options.scan_mode = ScanMode::kScanFull;
  const std::string json = ProbeReportToJson(ProbeMinimalIaSequence(options));

  EXPECT_THAT(json, HasSubstr("\"temporal_unit_scan\": {"));
  EXPECT_THAT(json, HasSubstr("\"stopped_reason\": \"eof\""));
  EXPECT_THAT(json, HasSubstr("\"audio_frames_by_substream\": ["));
}

TEST(ProbeReportToJson, EscapesStringsPerRfc8259) {
  ProbeReport report;
  report.primary_profile = "quote\"backslash\\control\x01";
  const std::string json = ProbeReportToJson(report);
  EXPECT_THAT(json, HasSubstr("\"primary_profile\": "
                              "\"quote\\\"backslash\\\\control\\u0001\""));
}

}  // namespace
}  // namespace iamf_tools
