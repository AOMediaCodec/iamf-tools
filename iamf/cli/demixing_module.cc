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
#include "iamf/cli/demixing_module.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <list>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using enum ChannelLabel::Label;
using DemixingMetadataForAudioElementId =
    DemixingModule::DemixingMetadataForAudioElementId;

absl::Status S7ToS5DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S7 to S5";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kL7) == label_to_samples.end() ||
      label_to_samples.find(kR7) == label_to_samples.end() ||
      label_to_samples.find(kLss7) == label_to_samples.end() ||
      label_to_samples.find(kLrs7) == label_to_samples.end() ||
      label_to_samples.find(kRss7) == label_to_samples.end() ||
      label_to_samples.find(kRrs7) == label_to_samples.end()) {
    return absl::InvalidArgumentError("Missing some input channels");
  }

  const auto& l7_samples = label_to_samples[kL7];
  const auto& lss7_samples = label_to_samples[kLss7];
  const auto& lrs7_samples = label_to_samples[kLrs7];
  const auto& r7_samples = label_to_samples[kR7];
  const auto& rss7_samples = label_to_samples[kRss7];
  const auto& rrs7_samples = label_to_samples[kRrs7];

  auto& l5_samples = label_to_samples[kL5];
  auto& r5_samples = label_to_samples[kR5];
  auto& ls5_samples = label_to_samples[kLs5];
  auto& rs5_samples = label_to_samples[kRs5];

  // Directly copy L7/R7 to L5/R5, because they are the same.
  l5_samples = l7_samples;
  r5_samples = r7_samples;

  // Handle Ls5 and Rs5.
  ls5_samples.resize(lss7_samples.size());
  rs5_samples.resize(rss7_samples.size());
  for (int i = 0; i < ls5_samples.size(); i++) {
    ls5_samples[i] = down_mixing_params.alpha * lss7_samples[i] +
                     down_mixing_params.beta * lrs7_samples[i];
    rs5_samples[i] = down_mixing_params.alpha * rss7_samples[i] +
                     down_mixing_params.beta * rrs7_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S5ToS7Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S5 to S7";

  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> ls5_samples;
  absl::Span<const InternalSampleType> lss7_samples;
  absl::Span<const InternalSampleType> r5_samples;
  absl::Span<const InternalSampleType> rs5_samples;
  absl::Span<const InternalSampleType> rss7_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kLs5, label_to_samples, ls5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kLss7, label_to_samples, lss7_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR5, label_to_samples, r5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kRs5, label_to_samples, rs5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kRss7, label_to_samples, rss7_samples));

  auto& l7_samples = label_to_samples[kDemixedL7];
  auto& r7_samples = label_to_samples[kDemixedR7];
  auto& lrs7_samples = label_to_samples[kDemixedLrs7];
  auto& rrs7_samples = label_to_samples[kDemixedRrs7];

  // Directly copy L5/R5 to L7/R7, because they are the same.
  l7_samples = {l5_samples.begin(), l5_samples.end()};
  r7_samples = {r5_samples.begin(), r5_samples.end()};

  // Handle Lrs7 and Rrs7.
  const size_t num_ticks = l5_samples.size();
  lrs7_samples.resize(num_ticks, 0.0);
  rrs7_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    lrs7_samples[i] =
        (ls5_samples[i] - down_mixing_params.alpha * lss7_samples[i]) /
        down_mixing_params.beta;
    rrs7_samples[i] =
        (rs5_samples[i] - down_mixing_params.alpha * rss7_samples[i]) /
        down_mixing_params.beta;
  }

  return absl::OkStatus();
}

absl::Status S5ToS3DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S5 to S3";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kL5) == label_to_samples.end() ||
      label_to_samples.find(kLs5) == label_to_samples.end() ||
      label_to_samples.find(kR5) == label_to_samples.end() ||
      label_to_samples.find(kRs5) == label_to_samples.end()) {
    return absl::InvalidArgumentError("Missing some input channels");
  }

  const auto& l5_samples = label_to_samples[kL5];
  const auto& ls5_samples = label_to_samples[kLs5];
  const auto& r5_samples = label_to_samples[kR5];
  const auto& rs5_samples = label_to_samples[kRs5];

  auto& l3_samples = label_to_samples[kL3];
  auto& r3_samples = label_to_samples[kR3];
  l3_samples.resize(l5_samples.size());
  r3_samples.resize(r5_samples.size());
  for (int i = 0; i < l3_samples.size(); i++) {
    l3_samples[i] = l5_samples[i] + down_mixing_params.delta * ls5_samples[i];
    r3_samples[i] = r5_samples[i] + down_mixing_params.delta * rs5_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S3ToS5Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S3 to S5";

  absl::Span<const InternalSampleType> l3_samples;
  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> r3_samples;
  absl::Span<const InternalSampleType> r5_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL3, label_to_samples, l3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR3, label_to_samples, r3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR5, label_to_samples, r5_samples));

  auto& ls5_samples = label_to_samples[kDemixedLs5];
  auto& rs5_samples = label_to_samples[kDemixedRs5];

  const size_t num_ticks = l3_samples.size();
  ls5_samples.resize(num_ticks, 0.0);
  rs5_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ls5_samples[i] = (l3_samples[i] - l5_samples[i]) / down_mixing_params.delta;
    rs5_samples[i] = (r3_samples[i] - r5_samples[i]) / down_mixing_params.delta;
  }

  return absl::OkStatus();
}

absl::Status S3ToS2DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S3 to S2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kL3) == label_to_samples.end() ||
      label_to_samples.find(kR3) == label_to_samples.end() ||
      label_to_samples.find(kCentre) == label_to_samples.end()) {
    return absl::InvalidArgumentError("Missing some input channels");
  }

  const auto& l3_samples = label_to_samples[kL3];
  const auto& r3_samples = label_to_samples[kR3];
  const auto& c_samples = label_to_samples[kCentre];

  auto& l2_samples = label_to_samples[kL2];
  auto& r2_samples = label_to_samples[kR2];
  l2_samples.resize(l3_samples.size());
  r2_samples.resize(r3_samples.size());
  for (int i = 0; i < l2_samples.size(); i++) {
    l2_samples[i] = l3_samples[i] + 0.707 * c_samples[i];
    r2_samples[i] = r3_samples[i] + 0.707 * c_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S2ToS3Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S2 to S3";

  absl::Span<const InternalSampleType> l2_samples;
  absl::Span<const InternalSampleType> r2_samples;
  absl::Span<const InternalSampleType> c_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL2, label_to_samples, l2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR2, label_to_samples, r2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kCentre, label_to_samples, c_samples));

  auto& l3_samples = label_to_samples[kDemixedL3];
  auto& r3_samples = label_to_samples[kDemixedR3];

  const size_t num_ticks = c_samples.size();
  l3_samples.resize(num_ticks, 0.0);
  r3_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    l3_samples[i] = (l2_samples[i] - 0.707 * c_samples[i]);
    r3_samples[i] = (r2_samples[i] - 0.707 * c_samples[i]);
  }

  return absl::OkStatus();
}

absl::Status S2ToS1DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S2 to S1";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kL2) == label_to_samples.end() ||
      label_to_samples.find(kR2) == label_to_samples.end()) {
    return absl::UnknownError("Missing some input channels");
  }

  const auto& l2_samples = label_to_samples[kL2];
  const auto& r2_samples = label_to_samples[kR2];

  auto& mono_samples = label_to_samples[kMono];
  mono_samples.resize(l2_samples.size());
  for (int i = 0; i < mono_samples.size(); i++) {
    mono_samples[i] = 0.5 * (l2_samples[i] + r2_samples[i]);
  }

  return absl::OkStatus();
}

absl::Status S1ToS2Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap& label_to_samples) {
  VLOG(1) << "S1 to S2";

  absl::Span<const InternalSampleType> l2_samples;
  absl::Span<const InternalSampleType> mono_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL2, label_to_samples, l2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kMono, label_to_samples, mono_samples));

  auto& r2_samples = label_to_samples[kDemixedR2];
  const size_t num_ticks = mono_samples.size();
  r2_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    r2_samples[i] = 2.0 * mono_samples[i] - l2_samples[i];
  }

  return absl::OkStatus();
}

absl::Status T4ToT2DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  VLOG(1) << "T4 to T2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kLtf4) == label_to_samples.end() ||
      label_to_samples.find(kLtb4) == label_to_samples.end() ||
      label_to_samples.find(kRtf4) == label_to_samples.end() ||
      label_to_samples.find(kRtb4) == label_to_samples.end()) {
    return absl::UnknownError("Missing some input channels");
  }

  const auto& ltf4_samples = label_to_samples[kLtf4];
  const auto& ltb4_samples = label_to_samples[kLtb4];
  const auto& rtf4_samples = label_to_samples[kRtf4];
  const auto& rtb4_samples = label_to_samples[kRtb4];

  auto& ltf2_samples = label_to_samples[kLtf2];
  auto& rtf2_samples = label_to_samples[kRtf2];
  ltf2_samples.resize(ltf4_samples.size());
  rtf2_samples.resize(rtf4_samples.size());
  for (int i = 0; i < ltf2_samples.size(); i++) {
    ltf2_samples[i] =
        ltf4_samples[i] + down_mixing_params.gamma * ltb4_samples[i];
    rtf2_samples[i] =
        rtf4_samples[i] + down_mixing_params.gamma * rtb4_samples[i];
  }

  return absl::OkStatus();
}

absl::Status T2ToT4Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  VLOG(1) << "T2 to T4";

  absl::Span<const InternalSampleType> ltf2_samples;
  absl::Span<const InternalSampleType> ltf4_samples;
  absl::Span<const InternalSampleType> rtf2_samples;
  absl::Span<const InternalSampleType> rtf4_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kLtf2, label_to_samples, ltf2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kLtf4, label_to_samples, ltf4_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kRtf2, label_to_samples, rtf2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kRtf4, label_to_samples, rtf4_samples));

  auto& ltb4_samples = label_to_samples[kDemixedLtb4];
  auto& rtb4_samples = label_to_samples[kDemixedRtb4];
  const size_t num_ticks = ltf2_samples.size();
  ltb4_samples.resize(num_ticks, 0.0);
  rtb4_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ltb4_samples[i] =
        (ltf2_samples[i] - ltf4_samples[i]) / down_mixing_params.gamma;
    rtb4_samples[i] =
        (rtf2_samples[i] - rtf4_samples[i]) / down_mixing_params.gamma;
  }

  return absl::OkStatus();
}

absl::Status T2ToTf2DownMixer(const DownMixingParams& down_mixing_params,
                              LabelSamplesMap& label_to_samples) {
  VLOG(1) << "T2 to TF2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples.find(kLtf2) == label_to_samples.end() ||
      label_to_samples.find(kLs5) == label_to_samples.end() ||
      label_to_samples.find(kRtf2) == label_to_samples.end() ||
      label_to_samples.find(kRs5) == label_to_samples.end()) {
    return absl::UnknownError("Missing some input channels");
  }

  const auto& ltf2_samples = label_to_samples[kLtf2];
  const auto& ls5_samples = label_to_samples[kLs5];
  const auto& rtf2_samples = label_to_samples[kRtf2];
  const auto& rs5_samples = label_to_samples[kRs5];

  auto& ltf3_samples = label_to_samples[kLtf3];
  auto& rtf3_samples = label_to_samples[kRtf3];
  ltf3_samples.resize(ltf2_samples.size());
  rtf3_samples.resize(rtf2_samples.size());
  for (int i = 0; i < ltf2_samples.size(); i++) {
    ltf3_samples[i] = ltf2_samples[i] + down_mixing_params.w *
                                            down_mixing_params.delta *
                                            ls5_samples[i];
    rtf3_samples[i] = rtf2_samples[i] + down_mixing_params.w *
                                            down_mixing_params.delta *
                                            rs5_samples[i];
  }

  return absl::OkStatus();
}

absl::Status Tf2ToT2Demixer(const DownMixingParams& down_mixing_params,
                            LabelSamplesMap& label_to_samples) {
  VLOG(1) << "TF2 to T2";

  absl::Span<const InternalSampleType> ltf3_samples;
  absl::Span<const InternalSampleType> l3_samples;
  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> rtf3_samples;
  absl::Span<const InternalSampleType> r3_samples;
  absl::Span<const InternalSampleType> r5_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kLtf3, label_to_samples, ltf3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL3, label_to_samples, l3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kRtf3, label_to_samples, rtf3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR3, label_to_samples, r3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      kR5, label_to_samples, r5_samples));

  auto& ltf2_samples = label_to_samples[kDemixedLtf2];
  auto& rtf2_samples = label_to_samples[kDemixedRtf2];
  const size_t num_ticks = ltf3_samples.size();
  ltf2_samples.resize(num_ticks, 0.0);
  rtf2_samples.resize(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ltf2_samples[i] = ltf3_samples[i] -
                      down_mixing_params.w * (l3_samples[i] - l5_samples[i]);
    rtf2_samples[i] = rtf3_samples[i] -
                      down_mixing_params.w * (r3_samples[i] - r5_samples[i]);
  }

  return absl::OkStatus();
}

// Helper to fill in the fields of `DemixingMetadataForAudioElementId`.
absl::Status FillRequiredDemixingMetadata(
    const absl::flat_hash_set<ChannelLabel::Label>& labels_to_demix,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const LabelGainMap& label_to_output_gain,
    DemixingMetadataForAudioElementId& demixing_metadata) {
  auto& down_mixers = demixing_metadata.down_mixers;
  auto& demixers = demixing_metadata.demixers;

  if (!down_mixers.empty() || !demixers.empty()) {
    return absl::UnknownError(
        "`FillRequiredDemixingMetadata()` should only be called once per Audio "
        "Element ID");
  }
  demixing_metadata.substream_id_to_labels = substream_id_to_labels;
  demixing_metadata.label_to_output_gain = label_to_output_gain;

  // Find the input surround number.
  int input_surround_number = 0;
  if (labels_to_demix.contains(kL7)) {
    input_surround_number = 7;
  } else if (labels_to_demix.contains(kL5)) {
    input_surround_number = 5;
  } else if (labels_to_demix.contains(kL3)) {
    input_surround_number = 3;
  } else if (labels_to_demix.contains(kL2)) {
    input_surround_number = 2;
  } else if (labels_to_demix.contains(kMono)) {
    input_surround_number = 1;
  }

  // Find the lowest output surround number.
  int output_lowest_surround_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       demixing_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kL7) != labels.end() &&
        output_lowest_surround_number > 7) {
      output_lowest_surround_number = 7;
    } else if (std::find(labels.begin(), labels.end(), kL5) != labels.end() &&
               output_lowest_surround_number > 5) {
      output_lowest_surround_number = 5;
    } else if (std::find(labels.begin(), labels.end(), kL3) != labels.end() &&
               output_lowest_surround_number > 3) {
      output_lowest_surround_number = 3;
    } else if (std::find(labels.begin(), labels.end(), kL2) != labels.end() &&
               output_lowest_surround_number > 2) {
      output_lowest_surround_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kMono) != labels.end() &&
               output_lowest_surround_number > 1) {
      output_lowest_surround_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }
  VLOG(1) << "Surround down-mixers from S" << input_surround_number << " to S"
          << output_lowest_surround_number << " needed:";
  for (int surround_number = input_surround_number;
       surround_number > output_lowest_surround_number; surround_number--) {
    if (surround_number == 7) {
      down_mixers.push_back(S7ToS5DownMixer);
      VLOG(1) << "  S7ToS5DownMixer added";
      demixers.push_front(S5ToS7Demixer);
      VLOG(1) << "  S5ToS7Demixer added";
    } else if (surround_number == 5) {
      down_mixers.push_back(S5ToS3DownMixer);
      VLOG(1) << "  S5ToS3DownMixer added";
      demixers.push_front(S3ToS5Demixer);
      VLOG(1) << "  S3ToS5Demixer added";
    } else if (surround_number == 3) {
      down_mixers.push_back(S3ToS2DownMixer);
      VLOG(1) << "  S3ToS2DownMixer added";
      demixers.push_front(S2ToS3Demixer);
      VLOG(1) << "  S2ToS3Demixer added";
    } else if (surround_number == 2) {
      down_mixers.push_back(S2ToS1DownMixer);
      VLOG(1) << "  S2ToS1DownMixer added";
      demixers.push_front(S1ToS2Demixer);
      VLOG(1) << "  S1ToS2Demixer added";
    }
  }

  // Find the input height number. Artificially defining the height number of
  // "TF2" as 1.
  int input_height_number = 0;
  if (labels_to_demix.contains(kLtf4)) {
    input_height_number = 4;
  } else if (labels_to_demix.contains(kLtf2)) {
    input_height_number = 2;
  } else if (labels_to_demix.contains(kLtf3)) {
    input_height_number = 1;
  }

  // Find the lowest output height number.
  int output_lowest_height_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       demixing_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kLtf4) != labels.end() &&
        output_lowest_height_number > 4) {
      output_lowest_height_number = 4;
    } else if (std::find(labels.begin(), labels.end(), kLtf2) != labels.end() &&
               output_lowest_height_number > 2) {
      output_lowest_height_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kLtf3) != labels.end() &&
               output_lowest_height_number > 1) {
      output_lowest_height_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }

  // Collect demixers in a separate list first and append the list to the
  // output later. Height demixers need to be in reverse order as height
  // down-mixers but should go after the surround demixers.
  VLOG(1) << "Height down-mixers from T" << input_height_number << " to "
          << (output_lowest_height_number == 2 ? "T2" : "TF3") << " needed:";
  std::list<Demixer> height_demixers;
  for (int height_number = input_height_number;
       height_number > output_lowest_height_number; height_number--) {
    if (height_number == 4) {
      down_mixers.push_back(T4ToT2DownMixer);
      VLOG(1) << "  T4ToT2DownMixer added";
      height_demixers.push_front(T2ToT4Demixer);
      VLOG(1) << "  T2ToT4Demixer added";
    } else if (height_number == 2) {
      down_mixers.push_back(T2ToTf2DownMixer);
      VLOG(1) << "  T2ToTf2DownMixer added";
      height_demixers.push_front(Tf2ToT2Demixer);
      VLOG(1) << "  Tf2ToT2Demixer added";
    }
  }
  demixers.splice(demixers.end(), height_demixers);

  return absl::OkStatus();
}

void ConfigureLabeledFrame(const AudioFrameWithData& audio_frame,
                           LabeledFrame& labeled_frame) {
  labeled_frame.end_timestamp = audio_frame.end_timestamp;
  labeled_frame.samples_to_trim_at_end =
      audio_frame.obu.header_.num_samples_to_trim_at_end;
  labeled_frame.samples_to_trim_at_start =
      audio_frame.obu.header_.num_samples_to_trim_at_start;
  labeled_frame.demixing_params = audio_frame.down_mixing_params;
}

absl::Span<const std::vector<InternalSampleType>> GetEncodedSamples(
    const AudioFrameWithData& audio_frame_with_data) {
  if (!audio_frame_with_data.encoded_samples.has_value()) {
    return {};
  }
  return absl::MakeConstSpan(*audio_frame_with_data.encoded_samples);
}

absl::Span<const std::vector<InternalSampleType>> GetDecodedSamples(
    const AudioFrameWithData& audio_frame_with_data) {
  return audio_frame_with_data.decoded_samples;
}

absl::Status PassThroughReconGainDataForDecodedAudioFrame(
    const AudioFrameWithData& decoded_audio_frame,
    LabeledFrame& labeled_decoded_frame) {
  if (decoded_audio_frame.audio_element_with_data == nullptr) {
    LOG_FIRST_N(INFO, 1)
        << "No audio element with data found, thus layer info is inaccessible.";
    return absl::OkStatus();
  }
  auto layout_config = std::get_if<ScalableChannelLayoutConfig>(
      &decoded_audio_frame.audio_element_with_data->obu.config_);
  if (layout_config == nullptr) {
    LOG_IF(INFO, decoded_audio_frame.start_timestamp == 0)
        << "No scalable channel layout config found, thus recon gain "
           "info is not necessary.";
    return absl::OkStatus();
  }
  auto& loudspeaker_layout_per_layer =
      labeled_decoded_frame.loudspeaker_layout_per_layer;
  loudspeaker_layout_per_layer.clear();
  loudspeaker_layout_per_layer.reserve(
      layout_config->channel_audio_layer_configs.size());
  for (const auto& channel_audio_layer_config :
       layout_config->channel_audio_layer_configs) {
    loudspeaker_layout_per_layer.push_back(
        channel_audio_layer_config.loudspeaker_layout);
  }
  labeled_decoded_frame.recon_gain_info_parameter_data =
      decoded_audio_frame.recon_gain_info_parameter_data;
  return absl::OkStatus();
}

absl::Status StoreSamplesForAudioElementId(
    bool use_decoded_samples,
    const std::list<AudioFrameWithData>& audio_frames_or_decoded_audio_frames,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    LabeledFrame& labeled_frame) {
  if (audio_frames_or_decoded_audio_frames.empty()) {
    return absl::OkStatus();
  }
  const InternalTimestamp common_start_timestamp =
      audio_frames_or_decoded_audio_frames.begin()->start_timestamp;

  for (auto& audio_frame : audio_frames_or_decoded_audio_frames) {
    const auto substream_id = audio_frame.obu.GetSubstreamId();
    auto substream_id_labels_iter = substream_id_to_labels.find(substream_id);
    if (substream_id_labels_iter == substream_id_to_labels.end()) {
      // This audio frame might belong to a different audio element; skip it.
      continue;
    }

    // Validate that the frames are all aligned in time.
    RETURN_IF_NOT_OK(CompareTimestamps(common_start_timestamp,
                                       audio_frame.start_timestamp,
                                       "In StoreSamplesForAudioElementId(): "));

    ConfigureLabeledFrame(audio_frame, labeled_frame);
    const auto& labels = substream_id_labels_iter->second;
    const auto input_samples = use_decoded_samples
                                   ? GetDecodedSamples(audio_frame)
                                   : GetEncodedSamples(audio_frame);
    if (input_samples.empty()) {
      return absl::InvalidArgumentError(
          "Input samples are not available for down-mixing.");
    }

    const auto num_channels = labels.size();
    RETURN_IF_NOT_OK(ValidateEqual(
        input_samples.size(), num_channels,
        "Decoded number of channels vs. expected number of channels"));

    int channel_index = 0;
    for (const auto& label : labels) {
      labeled_frame.label_to_samples[label] = input_samples[channel_index];
      channel_index++;
    }
    if (use_decoded_samples) {
      RETURN_IF_NOT_OK(PassThroughReconGainDataForDecodedAudioFrame(
          audio_frame, labeled_frame));
    }
  }

  return absl::OkStatus();
}

absl::Status ApplyDemixers(const std::list<Demixer>& demixers,
                           LabeledFrame& labeled_frame) {
  for (const auto& demixer : demixers) {
    RETURN_IF_NOT_OK(
        demixer(labeled_frame.demixing_params, labeled_frame.label_to_samples));
  }
  return absl::OkStatus();
}

absl::Status GetDemixerMetadata(
    const DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<DecodedUleb128,
                              DemixingMetadataForAudioElementId>&
        audio_element_id_to_demixing_metadata,
    const DemixingMetadataForAudioElementId*& demixing_metadata) {
  const auto iter =
      audio_element_id_to_demixing_metadata.find(audio_element_id);
  if (iter == audio_element_id_to_demixing_metadata.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Demxiing metadata for Audio Element ID= ", audio_element_id,
        " not found"));
  }
  demixing_metadata = &iter->second;
  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
LookupLabelsToReconstruct(const AudioElementObu& obu) {
  switch (obu.GetAudioElementType()) {
    using enum AudioElementObu::AudioElementType;
    case kAudioElementChannelBased: {
      const auto& channel_audio_layer_configs =
          std::get<ScalableChannelLayoutConfig>(obu.config_)
              .channel_audio_layer_configs;
      if (channel_audio_layer_configs.empty()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Expected non-empty channel audio layer configs for Audio "
            "Element ID= ",
            obu.GetAudioElementId()));
      }

      // Reconstruct the highest layer.
      return ChannelLabel::
          LookupLabelsToReconstructFromScalableLoudspeakerLayout(
              channel_audio_layer_configs.back().loudspeaker_layout,
              channel_audio_layer_configs.back().expanded_loudspeaker_layout);
    }
    case kAudioElementSceneBased:
      // OK. Ambisonics does not have any channels to be reconstructed.
      return absl::flat_hash_set<ChannelLabel::Label>{};
      break;
    default:
      return absl::UnimplementedError(absl::StrCat(
          "Unsupported audio element type= ", obu.GetAudioElementType()));
  }
}
void LogForAudioElementId(absl::string_view log_prefix,
                          DecodedUleb128 audio_element_id,
                          const IdLabeledFrameMap& id_to_labeled_frame) {
  if (!id_to_labeled_frame.contains(audio_element_id)) {
    return;
  }
  for (const auto& [label, samples] :
       id_to_labeled_frame.at(audio_element_id).label_to_samples) {
    VLOG(1) << "  Channel " << label << ":\t" << log_prefix
            << " frame size= " << samples.size() << ".";
  }
}

}  // namespace

absl::Status DemixingModule::FindSamplesOrDemixedSamples(
    ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
    absl::Span<const InternalSampleType>& samples) {
  if (label_to_samples.find(label) != label_to_samples.end()) {
    samples = absl::MakeConstSpan(label_to_samples.at(label));
    return absl::OkStatus();
  }

  auto demixed_label = ChannelLabel::GetDemixedLabel(label);
  if (!demixed_label.ok()) {
    return demixed_label.status();
  }
  if (label_to_samples.find(*demixed_label) != label_to_samples.end()) {
    samples = absl::MakeConstSpan(label_to_samples.at(*demixed_label));
    return absl::OkStatus();
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Channel ", label, " or ", *demixed_label, " not found"));
  }
}

absl::StatusOr<DemixingModule>
DemixingModule::CreateForDownMixingAndReconstruction(
    const absl::flat_hash_map<
        DecodedUleb128, DownmixingAndReconstructionConfig>&& id_to_config_map) {
  absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>
      audio_element_id_to_demixing_metadata;

  for (const auto& [audio_element_id, config] : id_to_config_map) {
    RETURN_IF_NOT_OK(FillRequiredDemixingMetadata(
        config.user_labels, config.substream_id_to_labels,
        config.label_to_output_gain,
        audio_element_id_to_demixing_metadata[audio_element_id]));
  }

  return DemixingModule(DemixingMode::kDownMixingAndReconstruction,
                        std::move(audio_element_id_to_demixing_metadata));
}

absl::StatusOr<DemixingModule> DemixingModule::CreateForReconstruction(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>
      audio_element_id_to_demixing_metadata;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    const auto labels_to_reconstruct =
        LookupLabelsToReconstruct(audio_element_with_data.obu);
    if (!labels_to_reconstruct.ok()) {
      return labels_to_reconstruct.status();
    }

    auto [iter, inserted] = audio_element_id_to_demixing_metadata.insert(
        {audio_element_id, DemixingMetadataForAudioElementId()});
    CHECK(inserted) << "The target map was initially empty, iterating over "
                       "`audio_elements` cannot produce a duplicate key.";
    RETURN_IF_NOT_OK(FillRequiredDemixingMetadata(
        *labels_to_reconstruct, audio_element_with_data.substream_id_to_labels,
        audio_element_with_data.label_to_output_gain, iter->second));
    iter->second.down_mixers.clear();
  }

  return DemixingModule(DemixingMode::kReconstruction,
                        std::move(audio_element_id_to_demixing_metadata));
}

absl::Status DemixingModule::DownMixSamplesToSubstreams(
    DecodedUleb128 audio_element_id, const DownMixingParams& down_mixing_params,
    LabelSamplesMap& input_label_to_samples,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) const {
  const DemixingMetadataForAudioElementId* demixing_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDemixerMetadata(audio_element_id,
                                      audio_element_id_to_demixing_metadata_,
                                      demixing_metadata));

  // First perform all the down mixing.
  for (const auto& down_mixer : demixing_metadata->down_mixers) {
    RETURN_IF_NOT_OK(down_mixer(down_mixing_params, input_label_to_samples));
  }

  for (const auto& [substream_id, output_channel_labels] :
       demixing_metadata->substream_id_to_labels) {
    // Find the `SubstreamData` with this `substream_id`.
    auto substream_data_iter =
        substream_id_to_substream_data.find(substream_id);
    if (substream_data_iter == substream_id_to_substream_data.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find substream data for substream ID= ", substream_id));
    }
    auto& substream_data = substream_data_iter->second;

    int channel_index = 0;
    for (const auto& output_channel_label : output_channel_labels) {
      // Compute and store the linear output gains for this channel.
      const auto gain_iter =
          demixing_metadata->label_to_output_gain.find(output_channel_label);
      const double output_gain_linear =
          (gain_iter == demixing_metadata->label_to_output_gain.end())
              ? 1.0
              : std::pow(10.0, gain_iter->second / 20.0);
      auto samples_iter = input_label_to_samples.find(output_channel_label);
      if (samples_iter == input_label_to_samples.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Samples do not exist for channel: ", output_channel_label));
      }
      const auto& input_samples = samples_iter->second;

      // Add all down mixed samples to both substream frames.
      for (const auto input_sample : input_samples) {
        substream_data.frames_in_obu.PushSample(channel_index, input_sample);

        // Apply output gains to the samples going to the encoder and also
        // convert the samples to 32-bit integers.
        int32_t attenuated_sample_int32 = 0;
        RETURN_IF_NOT_OK(NormalizedFloatingPointToInt32(
            input_sample / output_gain_linear, attenuated_sample_int32));
        substream_data.frames_to_encode.PushSample(channel_index,
                                                   attenuated_sample_int32);
      }

      channel_index++;
    }
  }

  return absl::OkStatus();
}

// TODO(b/288240600): Down-mix audio samples in a standalone function too.
absl::StatusOr<IdLabeledFrameMap> DemixingModule::DemixOriginalAudioSamples(
    const std::list<AudioFrameWithData>& audio_frames) const {
  if (demixing_mode_ == DemixingMode::kReconstruction) {
    return absl::FailedPreconditionError(
        "Demixing original audio samples is not available in reconstruction "
        "mode.");
  }
  IdLabeledFrameMap id_to_labeled_frame;
  for (const auto& [audio_element_id, demixing_metadata] :
       audio_element_id_to_demixing_metadata_) {
    // Process the original audio frames.
    LabeledFrame labeled_frame;
    RETURN_IF_NOT_OK(StoreSamplesForAudioElementId(
        /*use_decoded_samples=*/false, audio_frames,
        demixing_metadata.substream_id_to_labels, labeled_frame));
    if (!labeled_frame.label_to_samples.empty()) {
      RETURN_IF_NOT_OK(
          ApplyDemixers(demixing_metadata.demixers, labeled_frame));
      id_to_labeled_frame[audio_element_id] = std::move(labeled_frame);
    }

    LogForAudioElementId("Original", audio_element_id, id_to_labeled_frame);
  }

  return id_to_labeled_frame;
}

absl::StatusOr<IdLabeledFrameMap> DemixingModule::DemixDecodedAudioSamples(
    const std::list<AudioFrameWithData>& decoded_audio_frames) const {
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  for (const auto& [audio_element_id, demixing_metadata] :
       audio_element_id_to_demixing_metadata_) {
    // Process the decoded audio frames.
    LabeledFrame labeled_decoded_frame;
    RETURN_IF_NOT_OK(StoreSamplesForAudioElementId(
        /*use_decoded_samples=*/true, decoded_audio_frames,
        demixing_metadata.substream_id_to_labels, labeled_decoded_frame));
    if (!labeled_decoded_frame.label_to_samples.empty()) {
      RETURN_IF_NOT_OK(
          ApplyDemixers(demixing_metadata.demixers, labeled_decoded_frame));
      id_to_labeled_decoded_frame[audio_element_id] =
          std::move(labeled_decoded_frame);
    }

    LogForAudioElementId("Decoded", audio_element_id,
                         id_to_labeled_decoded_frame);
  }

  return id_to_labeled_decoded_frame;
}

absl::Status DemixingModule::GetDownMixers(
    DecodedUleb128 audio_element_id,
    const std::list<Demixer>*& down_mixers) const {
  const DemixingMetadataForAudioElementId* demixing_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDemixerMetadata(audio_element_id,
                                      audio_element_id_to_demixing_metadata_,
                                      demixing_metadata));
  down_mixers = &demixing_metadata->down_mixers;
  return absl::OkStatus();
}

absl::Status DemixingModule::GetDemixers(
    DecodedUleb128 audio_element_id,
    const std::list<Demixer>*& demixers) const {
  const DemixingMetadataForAudioElementId* demixing_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDemixerMetadata(audio_element_id,
                                      audio_element_id_to_demixing_metadata_,
                                      demixing_metadata));
  demixers = &demixing_metadata->demixers;
  return absl::OkStatus();
}

}  // namespace iamf_tools
