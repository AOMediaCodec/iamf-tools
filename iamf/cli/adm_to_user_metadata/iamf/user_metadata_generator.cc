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

#include "iamf/cli/adm_to_user_metadata/iamf/user_metadata_generator.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/iamf/ia_sequence_header_obu_metadata_handler.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf.h"
#include "iamf/cli/adm_to_user_metadata/iamf/mix_presentation_handler.h"
#include "iamf/cli/adm_to_user_metadata/iamf/test_vector_metadata_handler.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/user_metadata_builder/audio_frame_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/codec_config_obu_metadata_builder.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {
constexpr DecodedUleb128 kCodecConfigId = 0;
}

absl::Status UserMetadataGenerator::WriteUserMetadataToFile(
    bool write_binary_proto, const std::filesystem::path& file_path,
    const iamf_tools_cli_proto::UserMetadata& user_metadata) {
  const auto file_name =
      file_path /
      absl::StrCat(user_metadata.test_vector_metadata().file_name_prefix(),
                   write_binary_proto ? ".binpb" : ".textproto");

  std::ofstream output_file(file_name, std::ios::binary | std::ios::out);
  if (!output_file.is_open()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to open file_name= ", file_name.string()));
  }

  if (write_binary_proto) {
    output_file << user_metadata.SerializeAsString();
  } else {
    output_file << user_metadata.DebugString();
  }

  output_file.close();
  LOG(INFO) << file_name.string() << " generated successfully.";

  return absl::OkStatus();
}

absl::StatusOr<iamf_tools_cli_proto::UserMetadata>
UserMetadataGenerator::GenerateUserMetadata(
    iamf_tools_cli_proto::ProfileVersion profile_version,
    absl::string_view file_prefix) const {
  std::vector<std::string> audio_pack_format_ids;
  audio_pack_format_ids.reserve(adm_.audio_objects.size());
  for (auto audio_object : adm_.audio_objects) {
    audio_pack_format_ids.push_back(audio_object.audio_pack_format_id_refs[0]);
  }

  auto iamf =
      IAMF::Create(adm_, max_frame_duration_, format_info_.samples_per_sec);
  if (!iamf.ok()) {
    return iamf.status();
  }
  iamf_tools_cli_proto::UserMetadata user_metadata;

  // Generate test vector metadata.
  TestVectorMetadataHandler(file_prefix,
                            *user_metadata.mutable_test_vector_metadata());

  // Generate ia sequence header metadata.
  PopulateIaSequenceHeaderObuMetadata(
      profile_version, *user_metadata.add_ia_sequence_header_metadata());

  // Generate codec config obu metadata.
  user_metadata.mutable_codec_config_metadata()->Add(
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, iamf->num_samples_per_frame_,
          format_info_.bits_per_sample, format_info_.samples_per_sec));

  // Mapping of audio element
  constexpr int32_t kFirstAudioElementId = 0;
  if (adm_.audio_programmes.empty()) {
    if (const auto status =
            iamf->audio_element_metadata_builder_.PopulateAudioElementMetadata(
                kFirstAudioElementId, kCodecConfigId, iamf->input_layouts_[0],
                *user_metadata.add_audio_element_metadata());
        !status.ok()) {
      return status;
    };
  } else {
    for (const auto& [unused_audio_object_id, audio_element_id] :
         iamf->audio_object_to_audio_element_) {
      if (const auto status =
              iamf->audio_element_metadata_builder_
                  .PopulateAudioElementMetadata(
                      audio_element_id, kCodecConfigId,
                      iamf->input_layouts_[audio_element_id],
                      *user_metadata.add_audio_element_metadata());
          !status.ok()) {
        return status;
      }
    }
  }

  // Generate mix presentation obu metadata.

  if (adm_.audio_programmes.empty()) {
    constexpr int32_t kFirstMixPresentationId = 0;
    // Generate a mix presentation with the first audio object and default
    // loudness metadata.
    const std::vector<AudioObject>& audio_objects = {adm_.audio_objects[0]};

    if (const auto& status =
            iamf->mix_presentation_handler_.PopulateMixPresentation(
                kFirstMixPresentationId, audio_objects, LoudnessMetadata(),
                *user_metadata.add_mix_presentation_metadata());
        !status.ok()) {
      return status;
    }
  } else {
    for (const auto& [mix_presentation_id, audio_objects_and_metadata] :
         iamf->mix_presentation_id_to_audio_objects_and_metadata_) {
      if (const auto& status =
              iamf->mix_presentation_handler_.PopulateMixPresentation(
                  mix_presentation_id, audio_objects_and_metadata.audio_objects,
                  adm_.audio_programmes[audio_objects_and_metadata
                                            .original_audio_programme_index]
                      .loudness_metadata,
                  *user_metadata.add_mix_presentation_metadata());
          !status.ok()) {
        return status;
      }
    }
  }

  // Generate audio frame metadata.
  if (adm_.audio_programmes.empty()) {
    // The output files have suffixes starting from 1.
    static const absl::string_view kFirstFileSuffix = "1";

    if (const auto& status =
            AudioFrameMetadataBuilder::PopulateAudioFrameMetadata(
                absl::StrCat(file_prefix, "_converted", kFirstFileSuffix,
                             ".wav"),
                kFirstAudioElementId, iamf->input_layouts_[0],
                *user_metadata.add_audio_frame_metadata());
        !status.ok()) {
      return status;
    }
  } else {
    int32_t audio_pack_index = 0;
    for (const auto& [unused_audio_object_id, audio_element_id] :
         iamf->audio_object_to_audio_element_) {
      // The output files have suffixes starting from 1.
      const std::string wav_file_name =
          absl::StrCat(file_prefix, "_converted", audio_pack_index + 1, ".wav");
      if (const auto& status =
              AudioFrameMetadataBuilder::PopulateAudioFrameMetadata(
                  wav_file_name, audio_element_id,
                  iamf->input_layouts_[audio_pack_index++],
                  *user_metadata.add_audio_frame_metadata());
          !status.ok()) {
        return status;
      }
    }
  }

  return user_metadata;
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
