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

#ifndef CLI_CHANNEL_LABEL_H_
#define CLI_CHANNEL_LABEL_H_

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/obu/audio_element.h"

namespace iamf_tools {

/*!\brief Enums and static functions to help process channel labels.*/
class ChannelLabel {
 public:
  /*!\brief Labels associated with input or output channels.
   *
   * Labels naming conventions are based on the IAMF spec
   * (https://aomediacodec.github.io/iamf/#processing-downmixmatrix,
   * https://aomediacodec.github.io/iamf/#iamfgeneration-scalablechannelaudio-downmixmechanism).
   */
  enum Label {
    kOmitted,
    // Mono channels.
    kMono,
    // Stereo or binaural channels.
    kL2,
    kR2,
    kDemixedR2,
    // Centre channel common to several layouts
    // (e.g. 3.1.2, 5.x.y, 7.x.y).
    kCentre,
    // LFE channel common to several layouts (e.g. 3.1.2, 5.1.y, 7.1.y, 9.1.6).
    kLFE,
    // 3.1.2 surround channels.
    kL3,
    kR3,
    kLtf3,
    kRtf3,
    kDemixedL3,
    kDemixedR3,
    // 5.x.y surround channels.
    kL5,
    kR5,
    kLs5,
    kRs5,
    kLtf2,
    kRtf2,
    kDemixedL5,
    kDemixedR5,
    kDemixedLs5,
    kDemixedRs5,
    kDemixedLtf2,
    kDemixedRtf2,
    // 7.x.y surround channels.
    kL7,
    kR7,
    kLss7,
    kRss7,
    kLrs7,
    kRrs7,
    kLtf4,
    kRtf4,
    kLtb4,
    kRtb4,
    kDemixedL7,
    kDemixedR7,
    kDemixedLrs7,
    kDemixedRrs7,
    kDemixedLtb4,
    kDemixedRtb4,
    // 9.1.6 surround channels.
    kFLc,
    kFC,
    kFRc,
    kFL,
    kFR,
    kSiL,
    kSiR,
    kBL,
    kBR,
    kTpFL,
    kTpFR,
    kTpSiL,
    kTpSiR,
    kTpBL,
    kTpBR,
    // Ambisonics channels.
    kA0,
    kA1,
    kA2,
    kA3,
    kA4,
    kA5,
    kA6,
    kA7,
    kA8,
    kA9,
    kA10,
    kA11,
    kA12,
    kA13,
    kA14,
    kA15,
    kA16,
    kA17,
    kA18,
    kA19,
    kA20,
    kA21,
    kA22,
    kA23,
    kA24,
  };

  template <typename Sink>
  void AbslStringify(Sink& sink, Label e) {
    sink.Append(LabelToString(e));
  }

  /*!\brief Converts the input string to a `Label`.
   *
   * Channel Labels, e.g. "L2", "Ls5". For ambisonics, use "A{ACN number}",
   * e.g. "A0", "A13", but prefer using `AmbisonicsChannelNumberToLabel()`
   * instead.
   *
   * \param label Label to convert.
   * \return Converted label on success. A specific status on failure.
   */
  static absl::StatusOr<Label> StringToLabel(absl::string_view label);

  /*!\brief Fills a container of labels based on a container of strings.
   *
   * \param input_labels_strings Container of strings to convert.
   * \param outpt_labels Container to fill with the converted labels. The
   *     labels are inserted using the end iterator as a "hint".
   * \return `absl::OkStatus()` on success. An error if any labels fail to be
   *     converted. An error if any output labels are duplicate.
   */
  template <class InputContainer, class OutputContainer>
  static absl::Status FillLabelsFromStrings(
      const InputContainer& input_label_strings,
      OutputContainer& output_labels) {
    for (const auto& input_label_string : input_label_strings) {
      const auto label = ChannelLabel::StringToLabel(input_label_string);
      if (!label.ok()) {
        return label.status();
      }

      if (std::find(output_labels.begin(), output_labels.end(), *label) !=
          output_labels.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Duplicate output_label: ", *label,
            " when inserting from input_label= ", input_label_string));
      }
      output_labels.insert(output_labels.end(), *label);
    }

    return absl::OkStatus();
  }

  /*!\brief Converts the `Label` to an output string.
   *
   * \param label Label to convert.
   * \return Converted label.
   */
  static std::string LabelToString(Label label);

  /*!\brief Gets the channel label for an ambisonics channel number.
   *
   * \param ambisonics_channel_number Ambisonics channel number to get the label
   *    of.
   * \return Converted label. A specific status on failure.
   */
  static absl::StatusOr<Label> AmbisonicsChannelNumberToLabel(
      int ambisonics_channel_number);

  /*!\brief Returns the demixed version of a channel label.
   *
   * \param label Label to get the demixed version of.
   * \return Converted label on success. A specific status if the channel is not
   *     suitable for demixing. A specific status on other failures.
   */
  static absl::StatusOr<Label> GetDemixedLabel(Label label);

  /*!\brief Gets the channel ordering to use for the associated input layout.
   *
   * The output is ordered to agree with the "precomputed" EAR matrices.
   *
   * \param loudspeaker_layout Layout to get the channel ordering from.
   * \return Channel ordering associated with the layout if known. Or a specific
   *     status on failure.
   */
  static absl::StatusOr<std::vector<ChannelLabel::Label>>
  LookupEarChannelOrderFromScalableLoudspeakerLayout(
      const ChannelAudioLayerConfig::LoudspeakerLayout& loudspeaker_layout);

  /*!\brief Gets the labels related to reconstructing the input layout.
   *
   * Returns the labels that may be needed to reconstruct the
   * `loudspeaker_layout`. This function is useful when audio frames represent
   * channels which do agree with the `loudspeaker_layout`. Usually this occurs
   * when there are multiple layers in a scalable channel audio element.
   *
   * \param loudspeaker_layout Layout to get the labels to reconstruct from.
   * \return Labels to reconstruct the associated layout if known. Or a specific
   *     status on failure.
   */
  static absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
  LookupLabelsToReconstructFromScalableLoudspeakerLayout(
      const ChannelAudioLayerConfig::LoudspeakerLayout& loudspeaker_layout);
};

}  // namespace iamf_tools

#endif  // CLI_CHANNEL_LABEL_H_
