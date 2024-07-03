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
#include "iamf/cli/proto_to_obu/ia_sequence_header_generator.h"

#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/common/macros.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

namespace {
absl::Status CopyProfileVersion(
    iamf_tools_cli_proto::ProfileVersion metadata_profile_version,
    ProfileVersion& obu_profile_version) {
  switch (metadata_profile_version) {
    using enum iamf_tools_cli_proto::ProfileVersion;
    case PROFILE_VERSION_SIMPLE:
      obu_profile_version = ProfileVersion::kIamfSimpleProfile;
      return absl::OkStatus();
    case PROFILE_VERSION_BASE:
      obu_profile_version = ProfileVersion::kIamfBaseProfile;
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown profile version= ", metadata_profile_version));
  }
}

}  // namespace

absl::Status IaSequenceHeaderGenerator::Generate(
    std::optional<IASequenceHeaderObu>& ia_sequence_header_obu) const {
  // Skip generation if the `ia_sequence_header_metadata_` is not initialized.
  if (!ia_sequence_header_metadata_.has_primary_profile() ||
      !ia_sequence_header_metadata_.has_additional_profile()) {
    return absl::OkStatus();
  }

  // IA Sequence Header-related arguments.
  ProfileVersion primary_profile;
  RETURN_IF_NOT_OK(CopyProfileVersion(
      ia_sequence_header_metadata_.primary_profile(), primary_profile));
  ProfileVersion additional_profile;
  RETURN_IF_NOT_OK(CopyProfileVersion(
      ia_sequence_header_metadata_.additional_profile(), additional_profile));

  ia_sequence_header_obu.emplace(
      GetHeaderFromMetadata(ia_sequence_header_metadata_.obu_header()),
      ia_sequence_header_metadata_.ia_code(), primary_profile,
      additional_profile);

  ia_sequence_header_obu->PrintObu();
  return absl::OkStatus();
}

}  // namespace iamf_tools
