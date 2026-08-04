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
#include "iamf/cli/downmixer_factory.h"

#include <climits>
#include <list>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/downmixer.h"

namespace iamf_tools {

using Label = ChannelLabel::Label;
using enum ChannelLabel::Label;

absl::StatusOr<std::list<DownMixer>>
DownmixerFactory::CreateScalableChannelDownmixers(
    const absl::flat_hash_set<Label>& labels_to_downmix,
    const SubstreamIdLabelsMap& substream_id_to_labels) {
  std::list<DownMixer> down_mixers;

  // Find the input surround number.
  int input_surround_number = 0;
  if (labels_to_downmix.contains(kL7)) {
    input_surround_number = 7;
  } else if (labels_to_downmix.contains(kL5)) {
    input_surround_number = 5;
  } else if (labels_to_downmix.contains(kL3)) {
    input_surround_number = 3;
  } else if (labels_to_downmix.contains(kL2)) {
    input_surround_number = 2;
  } else if (labels_to_downmix.contains(kMono)) {
    input_surround_number = 1;
  }

  // Find the lowest output surround number.
  int output_lowest_surround_number = INT_MAX;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    if (absl::c_linear_search(labels, kL7) &&
        output_lowest_surround_number > 7) {
      output_lowest_surround_number = 7;
    } else if (absl::c_linear_search(labels, kL5) &&
               output_lowest_surround_number > 5) {
      output_lowest_surround_number = 5;
    } else if (absl::c_linear_search(labels, kL3) &&
               output_lowest_surround_number > 3) {
      output_lowest_surround_number = 3;
    } else if (absl::c_linear_search(labels, kL2) &&
               output_lowest_surround_number > 2) {
      output_lowest_surround_number = 2;
    } else if (absl::c_linear_search(labels, kMono) &&
               output_lowest_surround_number > 1) {
      output_lowest_surround_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }
  ABSL_VLOG(1) << "Surround down-mixers from S" << input_surround_number
               << " to S" << output_lowest_surround_number << " needed:";
  for (int surround_number = input_surround_number;
       surround_number > output_lowest_surround_number; surround_number--) {
    if (surround_number == 7) {
      down_mixers.push_back(S7ToS5DownMixer);
      ABSL_VLOG(1) << "  S7ToS5DownMixer added";
    } else if (surround_number == 5) {
      down_mixers.push_back(S5ToS3DownMixer);
      ABSL_VLOG(1) << "  S5ToS3DownMixer added";
    } else if (surround_number == 3) {
      down_mixers.push_back(S3ToS2DownMixer);
      ABSL_VLOG(1) << "  S3ToS2DownMixer added";
    } else if (surround_number == 2) {
      down_mixers.push_back(S2ToS1DownMixer);
      ABSL_VLOG(1) << "  S2ToS1DownMixer added";
    }
  }

  // Find the input height number. Artificially defining the height number of
  // "TF2" as 1.
  int input_height_number = 0;
  if (labels_to_downmix.contains(kLtf4)) {
    input_height_number = 4;
  } else if (labels_to_downmix.contains(kLtf2)) {
    input_height_number = 2;
  } else if (labels_to_downmix.contains(kLtf3)) {
    input_height_number = 1;
  }

  // Find the lowest output height number.
  int output_lowest_height_number = INT_MAX;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    if (absl::c_linear_search(labels, kLtf4) &&
        output_lowest_height_number > 4) {
      output_lowest_height_number = 4;
    } else if (absl::c_linear_search(labels, kLtf2) &&
               output_lowest_height_number > 2) {
      output_lowest_height_number = 2;
    } else if (absl::c_linear_search(labels, kLtf3) &&
               output_lowest_height_number > 1) {
      output_lowest_height_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }

  ABSL_VLOG(1) << "Height down-mixers from T" << input_height_number << " to "
               << (output_lowest_height_number == 2 ? "T2" : "TF3")
               << " needed:";
  for (int height_number = input_height_number;
       height_number > output_lowest_height_number; height_number--) {
    if (height_number == 4) {
      down_mixers.push_back(T4ToT2DownMixer);
      ABSL_VLOG(1) << "  T4ToT2DownMixer added";
    } else if (height_number == 2) {
      down_mixers.push_back(T2ToTf2DownMixer);
      ABSL_VLOG(1) << "  T2ToTf2DownMixer added";
    }
  }

  return down_mixers;
}

}  // namespace iamf_tools
