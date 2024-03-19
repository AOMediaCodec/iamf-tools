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
#include "iamf/cli/recon_gain_generator.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"

namespace iamf_tools {

namespace {

// Returns the Root Mean Square (RMS) power of input `samples`.
double ComputeSignalPower(const std::vector<int32_t>& samples) {
  double mean_square = 0.0;
  const double scale = 1.0 / static_cast<double>(samples.size());
  for (const int32_t s : samples) {
    mean_square += scale * static_cast<double>(s) * static_cast<double>(s);
  }
  return std::sqrt(mean_square);
}

// Find relevant samples. E.g. Computation of D_Lrs7 uses Ls5 and Lss7. Spec
// says "relevant mixed channel of the down-mixed audio for CL #i-1." So Level
// Mk is the signal power or Ls5. Lss7 is from CL #i and does not contribute to
// Level Mk.
absl::Status FindRelevantMixedSamples(
    const bool additional_logging, const std::string& label,
    const LabelSamplesMap& label_to_samples,
    const std::vector<int32_t>** relevant_mixed_samples) {
  static const absl::NoDestructor<absl::flat_hash_map<std::string, std::string>>
      kMixedLabels({{"D_L7", "L5"},
                    {"D_R7", "R5"},
                    {"D_Lrs7", "Ls5"},
                    {"D_Rrs7", "Rs5"},
                    {"D_Ltb4", "Ltf2"},
                    {"D_Rtb4", "Rtf2"},
                    {"D_L5", "L3"},
                    {"D_R5", "R3"},
                    {"D_Ls5", "L3"},
                    {"D_Rs5", "R3"},
                    {"D_Ltf2", "Ltf3"},
                    {"D_Rtf2", "Rtf3"},
                    {"D_L3", "L2"},
                    {"D_R3", "R2"},
                    {"D_L2", "M"},
                    {"D_R2", "M"}});

  std::string mixed_label;

  if (!LookupInMap(*kMixedLabels, label, mixed_label).ok()) {
    return absl::InvalidArgumentError(absl::StrCat("Unknown label= ", label));
  }

  LOG_IF(INFO, additional_logging)
      << "Relevant mixed samples has label: " << mixed_label;
  return DemixingModule::FindSamplesOrDemixedSamples(
      mixed_label, label_to_samples, relevant_mixed_samples);
}

}  // namespace

absl::Status ReconGainGenerator::ComputeReconGain(
    const std::string& label, const uint32_t audio_element_id,
    const int32_t start_timestamp, double& recon_gain) const {
  const auto& label_to_samples =
      id_to_time_to_labeled_frame_.at(audio_element_id)
          .at(start_timestamp)
          .label_to_samples;
  const auto& label_to_decoded_samples =
      id_to_time_to_labeled_decoded_frame_.at(audio_element_id)
          .at(start_timestamp)
          .label_to_samples;
  if (label_to_decoded_samples.find(label) == label_to_decoded_samples.end() ||
      label_to_samples.find(label) == label_to_samples.end()) {
    LOG(ERROR) << "Demixed channel: " << label << " not found";
    return absl::InvalidArgumentError("");
  }

  LOG_IF(INFO, additional_logging_)
      << "label_to_samples[" << label
      << "].size()= " << label_to_samples.at(label).size();
  LOG_IF(INFO, additional_logging_)
      << "label_to_decoded_samples[" << label
      << "].size()= " << label_to_decoded_samples.at(label).size();

  // Gather information about the original samples.
  const std::vector<int32_t>* original_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      label, label_to_samples, &original_samples));
  // Level Ok in the Spec.
  const double original_power = ComputeSignalPower(*original_samples);

  // If 10*log10(level Ok / maxL^2) is less than the first threshold value
  // (e.g. -80dB), Recon_Gain (k, i) = 0. Where, maxL = 32767 for 16bits.
  // TODO(b/289064747): Investigate `max_l_squared`. The input to
  //                    `ComputeReconGain` is left-justified `int32_t`. Maybe it
  //                    should be changed from (2^15)^2 to (2^31)^2?
  const double max_l_squared = 32767 * 32767;
  const double original_power_db = 10 * log10(original_power / max_l_squared);
  LOG_IF(INFO, additional_logging_) << "Level OK (db) " << original_power_db;
  if (original_power_db < -80) {
    recon_gain = 0;
    return absl::OkStatus();
  }

  // Gather information about mixed samples.
  const std::vector<int32_t>* relevant_mixed_samples;
  RETURN_IF_NOT_OK(FindRelevantMixedSamples(
      additional_logging_, label, label_to_samples, &relevant_mixed_samples));
  // Level Mk in the Spec.
  const double relevant_mixed_power =
      ComputeSignalPower(*relevant_mixed_samples);
  const double mixed_power_db =
      10 * log10(relevant_mixed_power / max_l_squared);
  LOG_IF(INFO, additional_logging_) << "Level MK (db) " << mixed_power_db;

  // If 10*log10(level Ok / level Mk ) is less than the second threshold
  // value (e.g. -6dB), Recon_Gain (k, i) is set to the value which makes
  // level Ok = Recon_Gain (k, i)^2 x level Dk.
  double original_mixed_ratio_db =
      10 * log10(original_power / relevant_mixed_power);
  LOG_IF(INFO, additional_logging_)
      << "Level Ok / Level Mk (db) " << original_mixed_ratio_db;

  // Otherwise, Recon_Gain (k, i) = 1.
  if (original_mixed_ratio_db >= -6) {
    recon_gain = 1;
    return absl::OkStatus();
  }

  // Gather information about the demixed samples.
  const std::vector<int32_t>* demixed_samples;
  RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
      label, label_to_decoded_samples, &demixed_samples));
  // Level Dk in the Spec.
  const double demixed_power = ComputeSignalPower(*demixed_samples);

  // Set recon gain to the value implied by the spec.
  double demixed_power_ratio_db = 10 * log10(demixed_power / mixed_power_db);
  LOG_IF(INFO, additional_logging_)
      << "Level DK (db) " << demixed_power_ratio_db;
  recon_gain = std::sqrt(original_power / demixed_power);

  return absl::OkStatus();
}

}  // namespace iamf_tools
