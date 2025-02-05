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

#ifndef CLI_PROTO_CONVERSION_CHANNEL_LABEL_UTILS_H_
#define CLI_PROTO_CONVERSION_CHANNEL_LABEL_UTILS_H_

#include <algorithm>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_frame.pb.h"

namespace iamf_tools {

class ChannelLabelUtils {
 public:
  /*!\brief Converts the input proto enum to a `Label`.
   *
   * \param proto_label Label to convert.
   * \return Converted label on success. A specific status on failure.
   */
  static absl::StatusOr<ChannelLabel::Label> ProtoToLabel(
      iamf_tools_cli_proto::ChannelLabel proto_label);

  /*!\brief Converts the input `ChanelLabel` to a proto enum
   *
   * \param label Label to convert.
   * \return Converted label on success. A specific status on failure.
   */
  static absl::StatusOr<iamf_tools_cli_proto::ChannelLabel> LabelToProto(
      ChannelLabel::Label label);

  /*!\brief Converts labels and fill the output container.
   *
   * Useful to convert containers of `std::string`, `absl::string_view`, or
   * `iamf_tools_cli_proto::ChannelLabel` proto enums to the
   * canonical internal representation.
   *
   * \param input_labels Container to convert.
   * \param output_labels Container to fill with the converted labels. The
   *        labels are inserted using the end iterator as a "hint"; when both
   *        containers are ordered the input and output order will agree.
   * \return `absl::OkStatus()` on success. An error if any labels fail to be
   *         converted. An error if any output labels are duplicate.
   */
  template <class InputContainer, class OutputContainer>
  static absl::Status ConvertAndFillLabels(const InputContainer& input_labels,
                                           OutputContainer& output_labels) {
    for (const auto& input_label : input_labels) {
      const absl::StatusOr<ChannelLabel::Label> label = [&]() -> auto {
        using iamf_tools_cli_proto::ChannelMetadata;
        if constexpr (std::is_convertible_v<decltype(input_label),
                                            absl::string_view>) {
          return ChannelLabel::DeprecatedStringBasedLabelToLabel(input_label);
        } else if constexpr (std::is_convertible_v<decltype(input_label),
                                                   ChannelMetadata>) {
          return ProtoToLabel(input_label.channel_label());
        } else {
          return ProtoToLabel(input_label);
        }
      }();
      if (!label.ok()) {
        return label.status();
      }

      if (std::find(output_labels.begin(), output_labels.end(), *label) !=
          output_labels.end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Duplicate output_label: ", *label,
                         " when inserting from input_label= ", input_label));
      }
      output_labels.insert(output_labels.end(), *label);
    }

    return absl::OkStatus();
  }

  /*!\brief Select the labels and forward to `ConvertAndFillLabels`.
   *
   * Acts as a shim to allow common handling of `channel_metadatas` and
   * the deprecated `channel_labels` fields. This function will change behavior
   * as the deprecation process moves forward.
   *
   * Prefers selecting labels based on the `channel_metadatas` field if it is
   * present. Warns, but permits the deprecated `channel_labels`. Forbids
   * partial upgrades, which would result in a confusing state with multiple
   * sources of labels.
   *
   * \param audio_frame_metadata Metadata to select labels from and convert.
   * \param output_labels Container to fill with the converted labels. The
   *        labels are inserted using the end iterator as a "hint"; when both
   *        containers are ordered the input and output order will agree.
   * \return `absl::OkStatus()` on success. An error if the `channel_metadatas`
   *         is present, but `channel_labels` or `channel_ids` is not empty. An
   *         error if any labels fail to be converted. An error if any output
   *         labels are duplicate.
   */
  template <class OutputContainer>
  static absl::Status SelectConvertAndFillLabels(
      const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata,
      OutputContainer& output_labels) {
    if (!audio_frame_metadata.channel_metadatas().empty()) {
      if (!audio_frame_metadata.channel_labels().empty()) {
        return absl::InvalidArgumentError(
            "Please fully upgrade to `channel_metadatas`. Leave "
            "`channel_labels` empty");
      }
      return ConvertAndFillLabels(audio_frame_metadata.channel_metadatas(),
                                  output_labels);
    } else {
      return ConvertAndFillLabels(audio_frame_metadata.channel_labels(),
                                  output_labels);
    }
  }
};
}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_CHANNEL_LABEL_UTILS_H_
