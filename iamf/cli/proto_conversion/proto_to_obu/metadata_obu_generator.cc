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

#include "iamf/cli/proto_conversion/proto_to_obu/metadata_obu_generator.h"

#include <list>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/proto/metadata_obu.pb.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

namespace {

MetadataObu CreateMetadataITUTT35(
    const iamf_tools_cli_proto::MetadataITUTT35& metadata_itu_t_t35) {
  MetadataITUTT35 metadata_itu_t_t35_obu;
  metadata_itu_t_t35_obu.itu_t_t35_country_code =
      metadata_itu_t_t35.itu_t_t35_country_code();
  if (metadata_itu_t_t35.has_itu_t_t35_country_code_extension_byte() &&
      metadata_itu_t_t35.itu_t_t35_country_code() == 0xff) {
    metadata_itu_t_t35_obu.itu_t_t35_country_code_extension_byte =
        metadata_itu_t_t35.itu_t_t35_country_code_extension_byte();
  }
  metadata_itu_t_t35_obu.itu_t_t35_payload_bytes = {
      metadata_itu_t_t35.itu_t_t35_payload_bytes().begin(),
      metadata_itu_t_t35.itu_t_t35_payload_bytes().end()};
  return MetadataObu::Create(ObuHeader{.obu_type = kObuIaMetadata},
                             std::move(metadata_itu_t_t35_obu));
}

using ::iamf_tools::MetadataIamfTags;
MetadataObu CreateMetadataIamfTags(
    const iamf_tools_cli_proto::MetadataIamfTags& metadata_iamf_tags) {
  MetadataIamfTags metadata_iamf_tags_obu;
  for (const auto& tag : metadata_iamf_tags.tags()) {
    metadata_iamf_tags_obu.tags.push_back(
        {.tag_name = tag.name(), .tag_value = tag.value()});
  }
  return MetadataObu::Create(ObuHeader{.obu_type = kObuIaMetadata},
                             std::move(metadata_iamf_tags_obu));
}

absl::StatusOr<MetadataObu> CreateMetadataObu(
    const iamf_tools_cli_proto::MetadataObuMetadata& metadata_obu_metadata) {
  if (metadata_obu_metadata.has_metadata_itu_t_t35()) {
    return CreateMetadataITUTT35(metadata_obu_metadata.metadata_itu_t_t35());
  } else if (metadata_obu_metadata.has_metadata_iamf_tags()) {
    return CreateMetadataIamfTags(metadata_obu_metadata.metadata_iamf_tags());
  } else {
    return absl::InvalidArgumentError(
        "MetadataObuMetadata must have one of the metadata fields set.");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status MetadataObuGenerator::Generate(
    std::list<MetadataObu>& metadata_obus) {
  for (const auto& metadata_obu_metadata : metadata_obu_metadata_) {
    absl::StatusOr<MetadataObu> metadata_obu =
        CreateMetadataObu(metadata_obu_metadata);
    if (!metadata_obu.ok()) {
      return metadata_obu.status();
    }
    metadata_obus.push_back(*std::move(metadata_obu));
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
