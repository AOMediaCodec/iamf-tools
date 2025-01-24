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

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <istream>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/adm_to_user_metadata/adm/wav_file_splicer.h"
#include "iamf/cli/adm_to_user_metadata/iamf/user_metadata_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

// ADM audioPackFormatID corresponding to 3rd order ambisonics, in which "0004"
// denotes type definition Ambisonics and "0003" denotes order 3.
constexpr char kAudioPackFormatIdFor3OA[] = "AP_00040003";
// A dummy audioPackFormatID created to represent typeDefinition as
// DirectSpeakers (0001) and layout as LFE (1FFF).
constexpr char kAudioPackFormatIdForLfe[] = "AP_00011FFF";

void ModifyAdmToPanObjectsTo3OAAndSeparateLfe(
    const ProfileVersion profile_version, const int lfe_count,
    ADM& adm_metadata) {
  using enum ProfileVersion;
  if (profile_version == kIamfBaseProfile) {
    // For IA Base Profile, max channels allowed per mix is 18, hence pan all
    // audio objects(both channel beds and objects) to 3OA (16 channels).
    adm_metadata.audio_objects.erase(adm_metadata.audio_objects.begin() + 1,
                                     adm_metadata.audio_objects.end());
    adm_metadata.audio_objects[0].audio_pack_format_id_refs[0] =
        kAudioPackFormatIdFor3OA;
  } else if (profile_version == kIamfBaseEnhancedProfile) {
    // For IA Base Enhanced Profile, max channels allowed per mix is 28, hence
    // pan all non-LFE channels(both channel beds and objects) in the to 3OA (16
    // channels) and keep the LFE(s) as separate audio element(s).
    adm_metadata.audio_objects.erase(
        adm_metadata.audio_objects.begin() + 1 + lfe_count,
        adm_metadata.audio_objects.end());
    adm_metadata.audio_objects[0].audio_pack_format_id_refs[0] =
        kAudioPackFormatIdFor3OA;
    for (int lfe_index = 1; lfe_index <= lfe_count; ++lfe_index) {
      adm_metadata.audio_objects[lfe_index].audio_pack_format_id_refs[0] =
          kAudioPackFormatIdForLfe;
    }
  }
}

}  // namespace

absl::StatusOr<iamf_tools_cli_proto::UserMetadata>
GenerateUserMetadataAndSpliceWavFiles(
    absl::string_view file_prefix, int32_t frame_duration_ms,
    int32_t input_importance_threshold,
    const std::filesystem::path& output_path, std::istream& adm_file,
    const iamf_tools::ProfileVersion profile_version) {
  // Parse the input ADM BWF file.
  const auto& reader =
      iamf_tools::adm_to_user_metadata::Bw64Reader::BuildFromStream(
          std::clamp(input_importance_threshold, 0, 10), adm_file);
  if (!reader.ok()) {
    return reader.status();
  }

  int lfe_count = 0;
  // Write output ".wav" file(s).
  RETURN_IF_NOT_OK(iamf_tools::adm_to_user_metadata::SpliceWavFilesFromAdm(
      output_path, file_prefix, profile_version, *reader, adm_file, lfe_count));

  ADM adm_metadata = reader->adm_;

  if (reader->adm_.file_type == kAdmFileTypeDolby) {
    ModifyAdmToPanObjectsTo3OAAndSeparateLfe(profile_version, lfe_count,
                                             adm_metadata);
  }

  // Generate the user metadata.
  const auto& user_metadata_generator =
      iamf_tools::adm_to_user_metadata::UserMetadataGenerator(
          adm_metadata, reader->format_info_, frame_duration_ms);

  using enum iamf_tools_cli_proto::ProfileVersion;
  using enum iamf_tools::ProfileVersion;
  iamf_tools_cli_proto::ProfileVersion version_for_proto;
  if (profile_version == kIamfBaseProfile) {
    version_for_proto = PROFILE_VERSION_BASE;
  } else if (profile_version == kIamfBaseEnhancedProfile) {
    version_for_proto = PROFILE_VERSION_BASE_ENHANCED;
  } else {
    return absl::InvalidArgumentError("Invalid profile version for proto.");
  }

  return user_metadata_generator.GenerateUserMetadata(version_for_proto,
                                                      file_prefix);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
