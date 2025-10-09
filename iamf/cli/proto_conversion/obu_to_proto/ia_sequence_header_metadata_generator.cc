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
#include "iamf/cli/proto_conversion/obu_to_proto/ia_sequence_header_metadata_generator.h"

#include <functional>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto_conversion/lookup_tables.h"
#include "iamf/cli/proto_conversion/obu_to_proto/obu_header_metadata_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

namespace {

typedef ::iamf_tools_cli_proto::IASequenceHeaderObuMetadata
    IASequenceHeaderObuMetadata;

typedef absl::FunctionRef<void(iamf_tools_cli_proto::ProfileVersion)>
    ProfileSetter;

absl::Status SetProfileVersion(ProfileVersion obu_profile_version,
                               ProfileSetter profile_setter) {
  // Only the test suite should use the reserved profile version.
  if (obu_profile_version == ProfileVersion::kIamfReserved255Profile) {
    return absl::InvalidArgumentError(
        "ProfileVersion::kIamfReserved255Profile is not supported.");
  }

  static const auto kInternalToProtoProfileVersion =
      BuildStaticMapFromInvertedPairs(
          LookupTables::kProtoAndInternalProfileVersions);

  return SetFromMap(*kInternalToProtoProfileVersion, obu_profile_version,
                    "Proto version of internal `ProfileVersion`",
                    profile_setter);
}

}  // namespace

absl::StatusOr<IASequenceHeaderObuMetadata>
IaSequenceHeaderMetadataGenerator::Generate(
    const IASequenceHeaderObu& ia_sequence_header) {
  IASequenceHeaderObuMetadata result;
  const auto obu_header =
      ObuHeaderMetadataGenerator::Generate(ia_sequence_header.header_);
  if (!obu_header.ok()) {
    return obu_header.status();
  }
  result.mutable_obu_header()->CopyFrom(*obu_header);

  result.set_ia_code(ia_sequence_header.GetIaCode());
  RETURN_IF_NOT_OK(SetProfileVersion(
      ia_sequence_header.GetPrimaryProfile(),
      std::bind_front(&IASequenceHeaderObuMetadata::set_primary_profile,
                      &result)));
  RETURN_IF_NOT_OK(SetProfileVersion(
      ia_sequence_header.GetAdditionalProfile(),
      std::bind_front(&IASequenceHeaderObuMetadata::set_additional_profile,
                      &result)));
  return result;
}

}  // namespace iamf_tools
