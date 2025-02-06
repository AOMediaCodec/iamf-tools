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
#include "iamf/cli/proto_conversion/proto_to_obu/ia_sequence_header_generator.h"

#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto_conversion/lookup_tables.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

namespace {

absl::Status ValidateProfileIsNotReserved(ProfileVersion obu_profile_version) {
  if (obu_profile_version == ProfileVersion::kIamfReserved255Profile) {
    return absl::InvalidArgumentError(
        "ProfileVersion::kIamfReserved255Profile is not supported.");
  }
  return absl::OkStatus();
}

absl::Status CopyProfileVersion(
    iamf_tools_cli_proto::ProfileVersion metadata_profile_version,
    ProfileVersion& obu_profile_version) {
  static const auto kProtoToInternalProfileVersion =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalProfileVersions);

  RETURN_IF_NOT_OK(CopyFromMap(
      *kProtoToInternalProfileVersion, metadata_profile_version,
      "Internal version of proto `ProfileVersion`", obu_profile_version));

  // Only the test suite should use the reserved profile version.
  MAYBE_RETURN_IF_NOT_OK(ValidateProfileIsNotReserved(obu_profile_version));
  return absl::OkStatus();
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
