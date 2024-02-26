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
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"

namespace iamf_tools {

namespace {

absl::Status S7ToS5DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S7 to S5";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("L7") == label_to_samples->end() ||
      label_to_samples->find("R7") == label_to_samples->end() ||
      label_to_samples->find("Lss7") == label_to_samples->end() ||
      label_to_samples->find("Lrs7") == label_to_samples->end() ||
      label_to_samples->find("Rss7") == label_to_samples->end() ||
      label_to_samples->find("Rrs7") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& l7_samples = (*label_to_samples)["L7"];
  const auto& lss7_samples = (*label_to_samples)["Lss7"];
  const auto& lrs7_samples = (*label_to_samples)["Lrs7"];
  const auto& r7_samples = (*label_to_samples)["R7"];
  const auto& rss7_samples = (*label_to_samples)["Rss7"];
  const auto& rrs7_samples = (*label_to_samples)["Rrs7"];

  auto& l5_samples = (*label_to_samples)["L5"];
  auto& r5_samples = (*label_to_samples)["R5"];
  auto& ls5_samples = (*label_to_samples)["Ls5"];
  auto& rs5_samples = (*label_to_samples)["Rs5"];

  // Directly copy L7/R7 to L5/R5, because they are the same.
  l5_samples = l7_samples;
  r5_samples = r7_samples;

  // Handle Ls5 and Rs5.
  ls5_samples.resize(lss7_samples.size());
  rs5_samples.resize(rss7_samples.size());

  // Computation in double.
  std::vector<double> ls5_samples_double(ls5_samples.size(), 0.0);
  std::vector<double> rs5_samples_double(rs5_samples.size(), 0.0);
  for (int i = 0; i < ls5_samples.size(); i++) {
    ls5_samples_double[i] =
        down_mixing_params.alpha * static_cast<double>(lss7_samples[i]) +
        down_mixing_params.beta * static_cast<double>(lrs7_samples[i]);
    rs5_samples_double[i] =
        down_mixing_params.alpha * static_cast<double>(rss7_samples[i]) +
        down_mixing_params.beta * static_cast<double>(rrs7_samples[i]);
  }

  // Convert back to int32_t.
  for (int i = 0; i < ls5_samples.size(); i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(ls5_samples_double[i], ls5_samples[i]));
    RETURN_IF_NOT_OK(ClipDoubleToInt32(rs5_samples_double[i], rs5_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S5ToS7Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S5 to S7";

  const std::vector<int32_t>* l5_samples;
  const std::vector<int32_t>* ls5_samples;
  const std::vector<int32_t>* lss7_samples;
  const std::vector<int32_t>* r5_samples;
  const std::vector<int32_t>* rs5_samples;
  const std::vector<int32_t>* rss7_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L5", *label_to_samples, &l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Ls5", *label_to_samples, &ls5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Lss7", *label_to_samples, &lss7_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R5", *label_to_samples, &r5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Rs5", *label_to_samples, &rs5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Rss7", *label_to_samples, &rss7_samples));

  auto& l7_samples = (*label_to_samples)["D_L7"];
  auto& r7_samples = (*label_to_samples)["D_R7"];
  auto& lrs7_samples = (*label_to_samples)["D_Lrs7"];
  auto& rrs7_samples = (*label_to_samples)["D_Rrs7"];

  // Directly copy L5/R5 to L7/R7, because they are the same.
  l7_samples = *l5_samples;
  r7_samples = *r5_samples;

  // Handle Lrs7 and Rrs7.
  const size_t num_ticks = l5_samples->size();
  lrs7_samples.resize(num_ticks, 0.0);
  rrs7_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> lrs7_samples_double(num_ticks, 0.0);
  std::vector<double> rrs7_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    lrs7_samples_double[i] =
        (static_cast<double>((*ls5_samples)[i]) -
         down_mixing_params.alpha * static_cast<double>((*lss7_samples)[i])) /
        down_mixing_params.beta;
    rrs7_samples_double[i] =
        (static_cast<double>((*rs5_samples)[i]) -
         down_mixing_params.alpha * static_cast<double>((*rss7_samples)[i])) /
        down_mixing_params.beta;
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(lrs7_samples_double[i], lrs7_samples[i]));
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(rrs7_samples_double[i], rrs7_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S5ToS3DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S5 to S3";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("L5") == label_to_samples->end() ||
      label_to_samples->find("Ls5") == label_to_samples->end() ||
      label_to_samples->find("R5") == label_to_samples->end() ||
      label_to_samples->find("Rs5") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& l5_samples = (*label_to_samples)["L5"];
  const auto& ls5_samples = (*label_to_samples)["Ls5"];
  const auto& r5_samples = (*label_to_samples)["R5"];
  const auto& rs5_samples = (*label_to_samples)["Rs5"];

  auto& l3_samples = (*label_to_samples)["L3"];
  auto& r3_samples = (*label_to_samples)["R3"];
  l3_samples.resize(l5_samples.size());
  r3_samples.resize(r5_samples.size());

  // Computation in double.
  std::vector<double> l3_samples_double(l3_samples.size(), 0.0);
  std::vector<double> r3_samples_double(r3_samples.size(), 0.0);
  for (int i = 0; i < l3_samples.size(); i++) {
    l3_samples_double[i] =
        static_cast<double>(l5_samples[i]) +
        down_mixing_params.delta * static_cast<double>(ls5_samples[i]);
    r3_samples_double[i] =
        static_cast<double>(r5_samples[i]) +
        down_mixing_params.delta * static_cast<double>(rs5_samples[i]);
  }

  // Convert back to int32_t.
  for (int i = 0; i < l3_samples.size(); i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(l3_samples_double[i], l3_samples[i]));
    RETURN_IF_NOT_OK(ClipDoubleToInt32(r3_samples_double[i], r3_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S3ToS5Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S3 to S5";

  const std::vector<int32_t>* l3_samples;
  const std::vector<int32_t>* l5_samples;
  const std::vector<int32_t>* r3_samples;
  const std::vector<int32_t>* r5_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L3", *label_to_samples, &l3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L5", *label_to_samples, &l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R3", *label_to_samples, &r3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R5", *label_to_samples, &r5_samples));

  auto& ls5_samples = (*label_to_samples)["D_Ls5"];
  auto& rs5_samples = (*label_to_samples)["D_Rs5"];

  const size_t num_ticks = l3_samples->size();
  ls5_samples.resize(num_ticks, 0.0);
  rs5_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> ls5_samples_double(num_ticks, 0.0);
  std::vector<double> rs5_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ls5_samples_double[i] = (static_cast<double>((*l3_samples)[i]) -
                             static_cast<double>((*l5_samples)[i])) /
                            down_mixing_params.delta;
    rs5_samples_double[i] = (static_cast<double>((*r3_samples)[i]) -
                             static_cast<double>((*r5_samples)[i])) /
                            down_mixing_params.delta;
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(ls5_samples_double[i], ls5_samples[i]));
    RETURN_IF_NOT_OK(ClipDoubleToInt32(rs5_samples_double[i], rs5_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S3ToS2DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S3 to S2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("L3") == label_to_samples->end() ||
      label_to_samples->find("R3") == label_to_samples->end() ||
      label_to_samples->find("C") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& l3_samples = (*label_to_samples)["L3"];
  const auto& r3_samples = (*label_to_samples)["R3"];
  const auto& c_samples = (*label_to_samples)["C"];

  auto& l2_samples = (*label_to_samples)["L2"];
  auto& r2_samples = (*label_to_samples)["R2"];
  l2_samples.resize(l3_samples.size());
  r2_samples.resize(r3_samples.size());

  // Computation in double.
  std::vector<double> l2_samples_double(l2_samples.size(), 0.0);
  std::vector<double> r2_samples_double(r2_samples.size(), 0.0);
  for (int i = 0; i < l2_samples.size(); i++) {
    l2_samples_double[i] = static_cast<double>(l3_samples[i]) +
                           0.707 * static_cast<double>(c_samples[i]);
    r2_samples_double[i] = static_cast<double>(r3_samples[i]) +
                           0.707 * static_cast<double>(c_samples[i]);
  }

  // Convert back to int32_t.
  for (int i = 0; i < l2_samples.size(); i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(l2_samples_double[i], l2_samples[i]));
    RETURN_IF_NOT_OK(ClipDoubleToInt32(r2_samples_double[i], r2_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S2ToS3Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S2 to S3";

  const std::vector<int32_t>* l2_samples;
  const std::vector<int32_t>* r2_samples;
  const std::vector<int32_t>* c_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L2", *label_to_samples, &l2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R2", *label_to_samples, &r2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "C", *label_to_samples, &c_samples));

  auto& l3_samples = (*label_to_samples)["D_L3"];
  auto& r3_samples = (*label_to_samples)["D_R3"];

  const size_t num_ticks = c_samples->size();
  l3_samples.resize(num_ticks, 0.0);
  r3_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> l3_samples_double(num_ticks, 0.0);
  std::vector<double> r3_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    l3_samples_double[i] = (static_cast<double>((*l2_samples)[i]) -
                            0.707 * static_cast<double>((*c_samples)[i]));
    r3_samples_double[i] = (static_cast<double>((*r2_samples)[i]) -
                            0.707 * static_cast<double>((*c_samples)[i]));
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(l3_samples_double[i], l3_samples[i]));
    RETURN_IF_NOT_OK(ClipDoubleToInt32(r3_samples_double[i], r3_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S2ToS1DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S2 to S1";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("L2") == label_to_samples->end() ||
      label_to_samples->find("R2") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& l2_samples = (*label_to_samples)["L2"];
  const auto& r2_samples = (*label_to_samples)["R2"];

  auto& mono_samples = (*label_to_samples)["M"];
  mono_samples.resize(l2_samples.size());

  // Computation in double.
  std::vector<double> mono_samples_double(l2_samples.size(), 0.0);
  for (int i = 0; i < mono_samples_double.size(); i++) {
    mono_samples_double[i] = 0.5 * (static_cast<double>(l2_samples[i]) +
                                    static_cast<double>(r2_samples[i]));
  }

  // Convert back to int32_t.
  for (int i = 0; i < mono_samples.size(); i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(mono_samples_double[i], mono_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status S1ToS2Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "S1 to S2";

  const std::vector<int32_t>* l2_samples;
  const std::vector<int32_t>* mono_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L2", *label_to_samples, &l2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "M", *label_to_samples, &mono_samples));

  auto& r2_samples = (*label_to_samples)["D_R2"];
  const size_t num_ticks = mono_samples->size();
  r2_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> r2_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    r2_samples_double[i] = 2.0 * static_cast<double>((*mono_samples)[i]) -
                           static_cast<double>((*l2_samples)[i]);
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(ClipDoubleToInt32(r2_samples_double[i], r2_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status T4ToT2DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "T4 to T2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("Ltf4") == label_to_samples->end() ||
      label_to_samples->find("Ltb4") == label_to_samples->end() ||
      label_to_samples->find("Rtf4") == label_to_samples->end() ||
      label_to_samples->find("Rtb4") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& ltf4_samples = (*label_to_samples)["Ltf4"];
  const auto& ltb4_samples = (*label_to_samples)["Ltb4"];
  const auto& rtf4_samples = (*label_to_samples)["Rtf4"];
  const auto& rtb4_samples = (*label_to_samples)["Rtb4"];

  auto& ltf2_samples = (*label_to_samples)["Ltf2"];
  auto& rtf2_samples = (*label_to_samples)["Rtf2"];
  ltf2_samples.resize(ltf4_samples.size());
  rtf2_samples.resize(rtf4_samples.size());

  // Computation in double.
  std::vector<double> ltf2_samples_double(ltf2_samples.size(), 0.0);
  std::vector<double> rtf2_samples_double(rtf2_samples.size(), 0.0);
  for (int i = 0; i < ltf2_samples.size(); i++) {
    ltf2_samples_double[i] =
        static_cast<double>(ltf4_samples[i]) +
        down_mixing_params.gamma * static_cast<double>(ltb4_samples[i]);
    rtf2_samples_double[i] =
        static_cast<double>(rtf4_samples[i]) +
        down_mixing_params.gamma * static_cast<double>(rtb4_samples[i]);
  }

  // Clip and convert back to int32_t.
  for (int i = 0; i < ltf2_samples.size(); i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(ltf2_samples_double[i], ltf2_samples[i]));
  }
  for (int i = 0; i < rtf2_samples.size(); i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(rtf2_samples_double[i], rtf2_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status T2ToT4Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "T2 to T4";

  const std::vector<int32_t>* ltf2_samples;
  const std::vector<int32_t>* ltf4_samples;
  const std::vector<int32_t>* rtf2_samples;
  const std::vector<int32_t>* rtf4_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Ltf2", *label_to_samples, &ltf2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Ltf4", *label_to_samples, &ltf4_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Rtf2", *label_to_samples, &rtf2_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Rtf4", *label_to_samples, &rtf4_samples));

  auto& ltb4_samples = (*label_to_samples)["D_Ltb4"];
  auto& rtb4_samples = (*label_to_samples)["D_Rtb4"];
  const size_t num_ticks = ltf2_samples->size();
  ltb4_samples.resize(num_ticks, 0.0);
  rtb4_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> ltb4_samples_double(num_ticks, 0.0);
  std::vector<double> rtb4_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ltb4_samples_double[i] = (static_cast<double>((*ltf2_samples)[i]) -
                              static_cast<double>((*ltf4_samples)[i])) /
                             down_mixing_params.gamma;
    rtb4_samples_double[i] = (static_cast<double>((*rtf2_samples)[i]) -
                              static_cast<double>((*rtf4_samples)[i])) /
                             down_mixing_params.gamma;
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(ltb4_samples_double[i], ltb4_samples[i]));
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(rtb4_samples_double[i], rtb4_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status T2ToTf2DownMixer(const DownMixingParams& down_mixing_params,
                              LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "T2 to TF2";

  // Check input to perform this down-mixing exist.
  if (label_to_samples->find("Ltf2") == label_to_samples->end() ||
      label_to_samples->find("Ls5") == label_to_samples->end() ||
      label_to_samples->find("Rtf2") == label_to_samples->end() ||
      label_to_samples->find("Rs5") == label_to_samples->end()) {
    LOG(ERROR) << "Missing some input channels";
    return absl::UnknownError("");
  }

  const auto& ltf2_samples = (*label_to_samples)["Ltf2"];
  const auto& ls5_samples = (*label_to_samples)["Ls5"];
  const auto& rtf2_samples = (*label_to_samples)["Rtf2"];
  const auto& rs5_samples = (*label_to_samples)["Rs5"];

  auto& ltf3_samples = (*label_to_samples)["Ltf3"];
  auto& rtf3_samples = (*label_to_samples)["Rtf3"];
  ltf3_samples.resize(ltf2_samples.size());
  rtf3_samples.resize(rtf2_samples.size());

  // Computation in double.
  std::vector<double> ltf3_samples_double(ltf3_samples.size(), 0.0);
  std::vector<double> rtf3_samples_double(rtf3_samples.size(), 0.0);
  for (int i = 0; i < ltf2_samples.size(); i++) {
    ltf3_samples_double[i] = static_cast<double>(ltf2_samples[i]) +
                             down_mixing_params.w * down_mixing_params.delta *
                                 static_cast<double>(ls5_samples[i]);
    rtf3_samples_double[i] = static_cast<double>(rtf2_samples[i]) +
                             down_mixing_params.w * down_mixing_params.delta *
                                 static_cast<double>(rs5_samples[i]);
  }

  // Convert back to int32_t.
  for (int i = 0; i < ltf3_samples.size(); i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(ltf3_samples_double[i], ltf3_samples[i]));
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(rtf3_samples_double[i], rtf3_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status Tf2ToT2Demixer(const DownMixingParams& down_mixing_params,
                            LabelSamplesMap* const label_to_samples) {
  LOG_FIRST_N(INFO, 1) << "TF2 to T2";

  const std::vector<int32_t>* ltf3_samples;
  const std::vector<int32_t>* l3_samples;
  const std::vector<int32_t>* l5_samples;
  const std::vector<int32_t>* rtf3_samples;
  const std::vector<int32_t>* r3_samples;
  const std::vector<int32_t>* r5_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Ltf3", *label_to_samples, &ltf3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L3", *label_to_samples, &l3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "L5", *label_to_samples, &l5_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "Rtf3", *label_to_samples, &rtf3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R3", *label_to_samples, &r3_samples));
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      "R5", *label_to_samples, &r5_samples));

  auto& ltf2_samples = (*label_to_samples)["D_Ltf2"];
  auto& rtf2_samples = (*label_to_samples)["D_Rtf2"];
  const size_t num_ticks = ltf3_samples->size();
  ltf2_samples.resize(num_ticks, 0.0);
  rtf2_samples.resize(num_ticks, 0.0);

  // Computation in double.
  std::vector<double> ltf2_samples_double(num_ticks, 0.0);
  std::vector<double> rtf2_samples_double(num_ticks, 0.0);
  for (int i = 0; i < num_ticks; i++) {
    ltf2_samples_double[i] =
        static_cast<double>((*ltf3_samples)[i]) -
        down_mixing_params.w * (static_cast<double>((*l3_samples)[i]) -
                                static_cast<double>((*l5_samples)[i]));
    rtf2_samples_double[i] =
        static_cast<double>((*rtf3_samples)[i]) -
        down_mixing_params.w * (static_cast<double>((*r3_samples)[i]) -
                                static_cast<double>((*r5_samples)[i]));
  }

  // Convert back to int32_t.
  for (int i = 0; i < num_ticks; i++) {
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(ltf2_samples_double[i], ltf2_samples[i]));
    RETURN_IF_NOT_OK(
        ClipDoubleToInt32(rtf2_samples_double[i], rtf2_samples[i]));
  }

  return absl::OkStatus();
}

absl::Status FindRequiredDownMixers(
    const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    std::list<Demixer>& down_mixers, std::list<Demixer>& demixers) {
  if (!down_mixers.empty() || !demixers.empty()) {
    LOG(ERROR) << "`FindRequiredDownMixers()` should only be called once "
               << "per Audio Element ID";
    return absl::UnknownError("");
  }

  const auto& input_channel_ids = audio_frame_metadata.channel_ids();
  const auto& input_channel_labels = audio_frame_metadata.channel_labels();
  if (input_channel_ids.size() != input_channel_labels.size()) {
    LOG(ERROR) << "#channel IDs and #channel labels differ: ("
               << input_channel_ids.size() << " vs "
               << input_channel_labels.size() << ").";
    return absl::InvalidArgumentError("");
  }

  // Find the input surround number.
  int input_surround_number = 0;
  if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                "L7") != input_channel_labels.end()) {
    input_surround_number = 7;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "L5") != input_channel_labels.end()) {
    input_surround_number = 5;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "L3") != input_channel_labels.end()) {
    input_surround_number = 3;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "L2") != input_channel_labels.end()) {
    input_surround_number = 2;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "M") != input_channel_labels.end()) {
    input_surround_number = 1;
  }

  // Find the lowest output surround number.
  int output_lowest_surround_number = INT_MAX;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), "L7") != labels.end() &&
        output_lowest_surround_number > 7) {
      output_lowest_surround_number = 7;
    } else if (std::find(labels.begin(), labels.end(), "L5") != labels.end() &&
               output_lowest_surround_number > 5) {
      output_lowest_surround_number = 5;
    } else if (std::find(labels.begin(), labels.end(), "L3") != labels.end() &&
               output_lowest_surround_number > 3) {
      output_lowest_surround_number = 3;
    } else if (std::find(labels.begin(), labels.end(), "L2") != labels.end() &&
               output_lowest_surround_number > 2) {
      output_lowest_surround_number = 2;
    } else if (std::find(labels.begin(), labels.end(), "M") != labels.end() &&
               output_lowest_surround_number > 1) {
      output_lowest_surround_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }
  LOG(INFO) << "Surround down-mixers from S" << input_surround_number << " to S"
            << output_lowest_surround_number << " needed:";
  for (int surround_number = input_surround_number;
       surround_number > output_lowest_surround_number; surround_number--) {
    if (surround_number == 7) {
      down_mixers.push_back(S7ToS5DownMixer);
      LOG(INFO) << "  S7ToS5DownMixer added";
      demixers.push_front(S5ToS7Demixer);
      LOG(INFO) << "  S5ToS7Demixer added";
    } else if (surround_number == 5) {
      down_mixers.push_back(S5ToS3DownMixer);
      LOG(INFO) << "  S5ToS3DownMixer added";
      demixers.push_front(S3ToS5Demixer);
      LOG(INFO) << "  S3ToS5Demixer added";
    } else if (surround_number == 3) {
      down_mixers.push_back(S3ToS2DownMixer);
      LOG(INFO) << "  S3ToS2DownMixer added";
      demixers.push_front(S2ToS3Demixer);
      LOG(INFO) << "  S2ToS3Demixer added";
    } else if (surround_number == 2) {
      down_mixers.push_back(S2ToS1DownMixer);
      LOG(INFO) << "  S2ToS1DownMixer added";
      demixers.push_front(S1ToS2Demixer);
      LOG(INFO) << "  S1ToS2Demixer added";
    }
  }

  // Find the input height number. Artificially defining the height number of
  // "TF2" as 1.
  int input_height_number = 0;
  if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                "Ltf4") != input_channel_labels.end()) {
    input_height_number = 4;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "Ltf2") != input_channel_labels.end()) {
    input_height_number = 2;
  } else if (std::find(input_channel_labels.begin(), input_channel_labels.end(),
                       "Ltf3") != input_channel_labels.end()) {
    input_height_number = 1;
  }

  // Find the lowest output height number.
  int output_lowest_height_number = INT_MAX;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), "Ltf4") != labels.end() &&
        output_lowest_height_number > 4) {
      output_lowest_height_number = 4;
    } else if (std::find(labels.begin(), labels.end(), "Ltf2") !=
                   labels.end() &&
               output_lowest_height_number > 2) {
      output_lowest_height_number = 2;
    } else if (std::find(labels.begin(), labels.end(), "Ltf3") !=
                   labels.end() &&
               output_lowest_height_number > 1) {
      output_lowest_height_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }

  // Collect demixers in a separate list first and append the list to the
  // output later. Height demixers need to be in reverse order as height
  // down-mixers but should go after the surround demixers.
  LOG(INFO) << "Height down-mixers from T" << input_height_number << " to "
            << (output_lowest_height_number == 2 ? "T2" : "TF3") << " needed:";
  std::list<Demixer> height_demixers;
  for (int height_number = input_height_number;
       height_number > output_lowest_height_number; height_number--) {
    if (height_number == 4) {
      down_mixers.push_back(T4ToT2DownMixer);
      LOG(INFO) << "  T4ToT2DownMixer added";
      height_demixers.push_front(T2ToT4Demixer);
      LOG(INFO) << "  T2ToT4Demixer added";
    } else if (height_number == 2) {
      down_mixers.push_back(T2ToTf2DownMixer);
      LOG(INFO) << "  T2ToTf2DownMixer added";
      height_demixers.push_front(Tf2ToT2Demixer);
      LOG(INFO) << "  Tf2ToT2Demixer added";
    }
  }
  demixers.splice(demixers.end(), height_demixers);

  return absl::OkStatus();
}

absl::Status StoreSamplesForAudioElementId(
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<DecodedAudioFrame>& decoded_audio_frames,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    TimeLabeledFrameMap& time_to_labeled_frame,
    TimeLabeledFrameMap& time_to_labeled_decoded_frame) {
  auto audio_frame_iter = audio_frames.begin();
  auto decoded_audio_frame_iter = decoded_audio_frames.begin();
  for (; audio_frame_iter != audio_frames.end() &&
         decoded_audio_frame_iter != decoded_audio_frames.end();
       audio_frame_iter++, decoded_audio_frame_iter++) {
    const auto substream_id = audio_frame_iter->obu.GetSubstreamId();
    auto substream_id_labels_iter = substream_id_to_labels.find(substream_id);
    if (substream_id_labels_iter == substream_id_to_labels.end()) {
      // This audio frame might belong to a different audio element; skip it.
      continue;
    }
    if (decoded_audio_frame_iter->substream_id != substream_id) {
      LOG(ERROR) << "Substream ID mismatch: " << substream_id << " vs "
                 << decoded_audio_frame_iter->substream_id;
      return absl::InvalidArgumentError("");
    }
    const auto& labels = substream_id_labels_iter->second;
    if (audio_frame_iter->raw_samples[0].size() != labels.size() ||
        decoded_audio_frame_iter->decoded_samples[0].size() != labels.size()) {
      LOG(ERROR) << "Channel number mismatch: "
                 << audio_frame_iter->raw_samples[0].size() << " vs "
                 << decoded_audio_frame_iter->decoded_samples[0].size()
                 << " vs " << labels.size();
      return absl::InvalidArgumentError("");
    }

    int channel_index = 0;
    for (const auto& label : labels) {
      const size_t num_ticks = audio_frame_iter->raw_samples.size();
      const int32_t start_timestamp = audio_frame_iter->start_timestamp;
      if (decoded_audio_frame_iter->start_timestamp != start_timestamp) {
        LOG(ERROR) << "Start timestamp mismatch: " << start_timestamp << " vs "
                   << decoded_audio_frame_iter->start_timestamp;
        return absl::InvalidArgumentError("");
      }

      auto& labeled_frame = time_to_labeled_frame[start_timestamp];
      labeled_frame.end_timestamp = audio_frame_iter->end_timestamp;
      labeled_frame.samples_to_trim_at_end =
          audio_frame_iter->obu.header_.num_samples_to_trim_at_end;
      labeled_frame.samples_to_trim_at_start =
          audio_frame_iter->obu.header_.num_samples_to_trim_at_start;
      labeled_frame.demixing_params = audio_frame_iter->down_mixing_params;

      auto& labeled_decoded_frame =
          time_to_labeled_decoded_frame[start_timestamp];
      labeled_decoded_frame.end_timestamp =
          decoded_audio_frame_iter->end_timestamp;
      labeled_decoded_frame.samples_to_trim_at_end =
          decoded_audio_frame_iter->samples_to_trim_at_end;
      labeled_decoded_frame.samples_to_trim_at_start =
          decoded_audio_frame_iter->samples_to_trim_at_start;

      auto& samples = labeled_frame.label_to_samples[label];
      auto& decoded_samples = labeled_decoded_frame.label_to_samples[label];

      samples.resize(num_ticks, 0);
      decoded_samples.resize(num_ticks, 0);
      for (int t = 0; t < audio_frame_iter->raw_samples.size(); t++) {
        samples[t] = audio_frame_iter->raw_samples[t][channel_index];
        decoded_samples[t] =
            decoded_audio_frame_iter->decoded_samples[t][channel_index];
      }
      channel_index++;
    }
  }

  return absl::OkStatus();
}

absl::Status ApplyDemixers(const std::list<Demixer>& demixers,
                           TimeLabeledFrameMap* time_to_labeled_frame,
                           TimeLabeledFrameMap* time_to_labeled_decoded_frame) {
  for (auto& [time, labeled_frame] : *time_to_labeled_frame) {
    auto& labeled_decoded_frame = time_to_labeled_decoded_frame->at(time);
    for (const auto& demixer : demixers) {
      RETURN_IF_NOT_OK(demixer(labeled_frame.demixing_params,
                               &labeled_frame.label_to_samples));
      RETURN_IF_NOT_OK(demixer(labeled_frame.demixing_params,
                               &labeled_decoded_frame.label_to_samples));
    }
  }

  return absl::OkStatus();
}

absl::Status GetDownMixersOrDemixers(
    const absl::Status init_error, const DecodedUleb128 audio_element_id,
    const DemixingModule::DemixerMap& domn_mixer_or_demixer_map,
    const std::list<Demixer>*& result) {
  if (init_error != absl::OkStatus()) {
    return absl::InvalidArgumentError("");
  }

  const auto iter = domn_mixer_or_demixer_map.find(audio_element_id);
  if (iter == domn_mixer_or_demixer_map.end()) {
    LOG(ERROR) << "Down mixer of demixer for Audio Element ID= "
               << audio_element_id << " not found";
    return absl::InvalidArgumentError("");
  }
  result = &(iter->second);
  return absl::OkStatus();
}

}  // namespace

absl::Status DemixingModule::FindSamplesOrDemixedSamples(
    const std::string& label, const LabelSamplesMap& label_to_samples,
    const std::vector<int32_t>** samples) {
  if (label_to_samples.find(label) != label_to_samples.end()) {
    *samples = &label_to_samples.at(label);
    return absl::OkStatus();
  } else if (label_to_samples.find(absl::StrCat("D_", label)) !=
             label_to_samples.end()) {
    *samples = &label_to_samples.at(absl::StrCat("D_", label));
    return absl::OkStatus();
  } else {
    LOG(ERROR) << "Channel " << label << " or D_" << label << " not found";
    *samples = nullptr;
    return absl::UnknownError("");
  }
}

DemixingModule::DemixingModule(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements)
    : audio_elements_(audio_elements), init_status_(absl::OkStatus()) {
  for (const auto& audio_frame_metadata :
       user_metadata.audio_frame_metadata()) {
    const auto audio_element_id = audio_frame_metadata.audio_element_id();
    init_status_ = FindRequiredDownMixers(
        audio_frame_metadata,
        audio_elements.at(audio_element_id).substream_id_to_labels,
        down_mixer_map_[audio_element_id], demixer_map_[audio_element_id]);

    if (init_status_ != absl::OkStatus()) {
      LOG(ERROR) << "Initialization of `DemixingModule` failed; abort";
      break;
    }
  }
}

absl::Status DemixingModule::DownMixSamplesToSubstreams(
    DecodedUleb128 audio_element_id, const DownMixingParams& down_mixing_params,
    LabelSamplesMap& input_label_to_samples,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) const {
  if (init_status_ != absl::OkStatus()) {
    LOG(ERROR) << "Cannot call `DownMixSamplesToSubstreams()` when "
               << "initialization failed.";
    return init_status_;
  }
  const auto& audio_element_with_data = audio_elements_.at(audio_element_id);
  const std::list<Demixer>* down_mixers;
  RETURN_IF_NOT_OK(GetDownMixers(audio_element_id, down_mixers));

  // First perform all the down mixing.
  for (const auto& down_mixer : *down_mixers) {
    RETURN_IF_NOT_OK(down_mixer(down_mixing_params, &input_label_to_samples));
  }

  const size_t num_time_ticks = input_label_to_samples.begin()->second.size();

  for (const auto& [substream_id, output_channel_labels] :
       audio_element_with_data.substream_id_to_labels) {
    std::vector<std::vector<int32_t>> substream_samples(
        num_time_ticks,
        // One or two channels.
        std::vector<int32_t>(output_channel_labels.size(), 0));
    // Output gains to be applied to the (one or two) channels.
    std::vector<double> output_gains_linear(output_channel_labels.size());
    int channel_index = 0;
    for (const std::string& output_channel_label : output_channel_labels) {
      auto iter = input_label_to_samples.find(output_channel_label);
      if (iter == input_label_to_samples.end()) {
        LOG(ERROR) << "Samples do not exist for channel: "
                   << output_channel_label;
        return absl::UnknownError("");
      }
      for (int t = 0; t < num_time_ticks; t++) {
        substream_samples[t][channel_index] = iter->second[t];
      }

      // Compute and store the linear output gains.
      auto gain_iter = audio_element_with_data.label_to_output_gain.find(
          output_channel_label);
      output_gains_linear[channel_index] = 1.0;
      if (gain_iter != audio_element_with_data.label_to_output_gain.end()) {
        output_gains_linear[channel_index] =
            std::pow(10.0, gain_iter->second / 20.0);
      }

      channel_index++;
    }

    // Find the `SubstreamData` with this `substream_id`.
    auto substream_data_iter =
        substream_id_to_substream_data.find(substream_id);
    if (substream_data_iter == substream_id_to_substream_data.end()) {
      LOG(ERROR) << "Failed to find subtream data for substream ID= "
                 << substream_id;
      return absl::UnknownError("");
    }
    auto& substream_data = substream_data_iter->second;

    // Add all down mixed samples to both queues.

    for (const auto& channel_samples : substream_samples) {
      substream_data.samples_obu.push_back(channel_samples);

      // Apply output gains to the samples going to the encoder.
      std::vector<int32_t> attenuated_channel_samples(channel_samples.size());
      for (int i = 0; i < channel_samples.size(); ++i) {
        RETURN_IF_NOT_OK(ClipDoubleToInt32(
            static_cast<double>(channel_samples[i]) / output_gains_linear[i],
            attenuated_channel_samples[i]));
      }
      substream_data.samples_encode.push_back(attenuated_channel_samples);
    }
  }

  return absl::OkStatus();
}

// TODO(b/288240600): Down-mix audio samples in a standalone function too.
absl::Status DemixingModule::DemixAudioSamples(
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<DecodedAudioFrame>& decoded_audio_frames,
    IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
    IdTimeLabeledFrameMap& id_to_time_to_labeled_decoded_frame) const {
  if (init_status_ != absl::OkStatus()) {
    LOG(ERROR) << "Cannot call `DemixAudioSamples()` when initialization "
               << "failed.";
    return init_status_;
  }
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements_) {
    auto& time_to_labeled_frame = id_to_time_to_labeled_frame[audio_element_id];
    auto& time_to_labeled_decoded_frame =
        id_to_time_to_labeled_decoded_frame[audio_element_id];

    RETURN_IF_NOT_OK(StoreSamplesForAudioElementId(
        audio_frames, decoded_audio_frames,
        audio_element_with_data.substream_id_to_labels, time_to_labeled_frame,
        time_to_labeled_decoded_frame));
    const std::list<Demixer>* demixers;
    RETURN_IF_NOT_OK(GetDemixers(audio_element_id, demixers));
    RETURN_IF_NOT_OK(ApplyDemixers(*demixers, &time_to_labeled_frame,
                                   &time_to_labeled_decoded_frame));

    LOG(INFO) << "Demixing Audio Element ID= " << audio_element_id;
    LOG(INFO) << "  Samples has " << time_to_labeled_frame.size() << " frames";
    LOG(INFO) << "  Decoded Samples has "
              << time_to_labeled_decoded_frame.size() << " frames";
    if (!time_to_labeled_frame.empty() &&
        !time_to_labeled_decoded_frame.empty()) {
      for (const auto& [label, samples] :
           time_to_labeled_frame.begin()->second.label_to_samples) {
        const auto& decoded_samples = time_to_labeled_decoded_frame.begin()
                                          ->second.label_to_samples[label];
        LOG(INFO) << "  Channel " << label
                  << ":\tframe size= " << samples.size()
                  << "; decoded frame size= " << decoded_samples.size();
      }
    }
  }

  return absl::OkStatus();
}

absl::Status DemixingModule::GetDownMixers(
    DecodedUleb128 audio_element_id,
    const std::list<Demixer>*& down_mixers) const {
  return GetDownMixersOrDemixers(init_status_, audio_element_id,
                                 down_mixer_map_, down_mixers);
}

absl::Status DemixingModule::GetDemixers(
    DecodedUleb128 audio_element_id,
    const std::list<Demixer>*& demixers) const {
  return GetDownMixersOrDemixers(init_status_, audio_element_id, demixer_map_,
                                 demixers);
}

}  // namespace iamf_tools
