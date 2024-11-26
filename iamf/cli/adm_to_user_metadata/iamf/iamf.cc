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

#include "iamf/cli/adm_to_user_metadata/iamf/iamf.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/iamf/mix_presentation_handler.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

constexpr uint64_t kMaxAudioElementPerMix = 2;

struct HasComplementaryObjectIdAndAudioObjectGroup {
  bool has_complementary_object_group;
  std::vector<std::string> audio_objects_ref_ids;
};

// This method maps the audio object groups to audio programme through recursive
// call. Each group represents a set of audio objects (one or two) that form a
// mix. This function returns a 2D vector, where the first dimension represents
// the audio object group.
std::vector<std::vector<std::string>> GenerateAudioObjectGroups(
    std::vector<HasComplementaryObjectIdAndAudioObjectGroup>&
        audio_object_groups) {
  std::vector<std::vector<std::string>> output = {};

  // All the audio_object_groups are mapped to audio programme.
  if (audio_object_groups.empty()) {
    std::vector<std::string> empty_entry = {};
    output.push_back(empty_entry);
    return output;
  }

  // Remove one audio object group to process.
  const auto audio_object_group = audio_object_groups.back();
  audio_object_groups.pop_back();
  if (audio_object_group.has_complementary_object_group) {
    // The audio object group has audio objects with complementary objects.
    for (const auto& audio_object_ref_id :
         audio_object_group.audio_objects_ref_ids) {
      // Get an audio programme to map the audio objects when the current group
      // is not present in audio object groups.
      const auto& audio_programme_to_audio_objects_list =
          GenerateAudioObjectGroups(audio_object_groups);

      // Append the current audio object group with audio object group list
      // obtained before.
      for (const auto& audio_programme_to_audio_object_ref_id :
           audio_programme_to_audio_objects_list) {
        output.push_back(audio_programme_to_audio_object_ref_id);
        output.back().push_back(audio_object_ref_id);
      }
    }
  } else {
    // The audio object group has an audio object.
    const auto& audio_programme_to_audio_objects_list =
        GenerateAudioObjectGroups(audio_object_groups);

    // Append the current audio object with audio object group list obtained
    // before.
    for (const auto& audio_programme_to_audio_object_entry :
         audio_programme_to_audio_objects_list) {
      output.push_back(audio_programme_to_audio_object_entry);
      output.back().push_back(audio_object_group.audio_objects_ref_ids[0]);
    }
  }
  return output;
}

// This method computes the number of audioProgramme(s) and the associated
// audioObject(s). If an audioProgramme contains an audioObject with a
// complementary audioObject, the complementary audioObject should be considered
// as part of a new audioProgramme to facilitate its representation in IAMF as
// two different mixes.
void GenerateAudioObjectsMap(
    const ADM& adm,
    std::map<int32_t, IAMF::AudioObjectsAndMetadata>&
        mix_presentation_id_to_audio_objects_and_metadata,
    std::map<std::string, uint32_t>& audio_object_to_audio_element) {
  // This vector contains audio object groups. An audio object group can have
  // one member if audio object inserted has no complementary audio objects,
  // otherwise audio object and all it's complementary audio objects form a
  // group.
  std::vector<HasComplementaryObjectIdAndAudioObjectGroup> audio_object_groups;

  // The following set contains audio objects which are already present in some
  // audio object group.
  absl::flat_hash_set<std::string> audio_object_ids_to_ignore;
  // Loop over the audioProgramme(s) present.
  for (uint64_t original_program_index = 0;
       original_program_index < adm.audio_programmes.size();
       ++original_program_index) {
    const auto& audio_programme = adm.audio_programmes[original_program_index];

    // Loop over the audioContent(s) within an audioProgramme.
    for (const auto& audio_content_id : audio_programme.audio_content_id_refs) {
      // Find the audio content in list of audio content in ADM.
      for (const auto& audio_content : adm.audio_contents) {
        absl::string_view current_audio_content_id = audio_content.id;
        if (current_audio_content_id != audio_content_id) {
          continue;
        }

        // Loop over the audioObject(s) within an audioContent
        for (const auto& audio_object_id : audio_content.audio_object_id_ref) {
          // Find the audio object in list of audio objects in ADM.
          for (const auto& audio_object : adm.audio_objects) {
            if (audio_object.id != audio_object_id) {
              continue;
            }
            // This audio object is already added, hence ignoring it.
            if (audio_object_ids_to_ignore.contains(audio_object.id)) {
              continue;
            }

            // Create the audio object group
            HasComplementaryObjectIdAndAudioObjectGroup audio_object_group{
                .has_complementary_object_group = false,
                .audio_objects_ref_ids = {audio_object.id}};

            if (!(audio_object.audio_comple_object_id_ref.empty())) {
              // Audio object has complementary objects and so push all of them
              // together form one entry of audio object groups
              audio_object_group.has_complementary_object_group = true;

              for (const auto& complementary_audio_object_id :
                   audio_object.audio_comple_object_id_ref) {
                audio_object_group.audio_objects_ref_ids.push_back(
                    complementary_audio_object_id);
                audio_object_ids_to_ignore.insert(
                    complementary_audio_object_id);
              }
            }
            audio_object_groups.push_back(audio_object_group);
          }
        }
      }
    }

    // Remove the unsupported audio programme.
    if (audio_object_groups.size() > kMaxAudioElementPerMix) {
      // TODO(b/331198492): Fix a bug where all subsequent programmes are
      //                    skipped - even if they would otherwise be valid.
      std::cout << "Skipping the audioProgramme" << audio_programme.id
                << "as the number of audioObjects is greater than "
                   "kMaxAudioElementPerMix"
                << std::endl;
      continue;
    }
    // Generate the audio object groups for the given audio programme.
    const auto& audio_program_to_audio_object_ids_map =
        GenerateAudioObjectGroups(audio_object_groups);

    // Push the audio object in a audio programme to audio object map with the
    // help of audio object ids
    for (const auto& audio_object_ids_map :
         audio_program_to_audio_object_ids_map) {
      std::vector<AudioObject> audio_objects_list;
      for (const auto& audio_object_id : audio_object_ids_map) {
        // Assign the IDs based on the order they were  were first found. It is
        // OK if an object is present in multiple programs.
        audio_object_to_audio_element.insert(
            {audio_object_id, audio_object_to_audio_element.size()});

        for (const auto& audio_object : adm.audio_objects) {
          if (audio_object.id == audio_object_id) {
            audio_objects_list.push_back(audio_object);
          }
        }
      }
      mix_presentation_id_to_audio_objects_and_metadata
          [mix_presentation_id_to_audio_objects_and_metadata.size()] = {
              audio_objects_list, static_cast<int32_t>(original_program_index)};
    }
    audio_object_ids_to_ignore.clear();
    audio_object_groups.clear();
  }
}

// Computes the number of samples per frame to correspond to a frame duration of
// at most `max_frame_duration_ms`.
absl::Status ComputeNumSamplesPerFrame(uint32_t max_frame_duration_ms,
                                       uint32_t num_samples_per_sec,
                                       int64_t& num_samples_per_frame) {
  if (num_samples_per_sec == 0 || max_frame_duration_ms == 0) {
    return absl::InvalidArgumentError(
        "Cannot compute number of samples per frame.");
  }

  // Round down to the nearest integer. This ensures the actual number of
  // samples per frame corresponds to a duration no greater than the requested
  // duration.
  num_samples_per_frame = static_cast<int64_t>(max_frame_duration_ms *
                                               num_samples_per_sec / 1000.0);

  return absl::OkStatus();
}

}  // namespace

IAMF::IAMF(const std::map<int32_t, AudioObjectsAndMetadata>&
               mix_presentation_id_to_audio_objects_and_metadata,
           const std::map<std::string, uint32_t>& audio_object_to_audio_element,
           int64_t num_samples_per_frame, uint32_t samples_per_sec,
           const std::vector<IamfInputLayout>& input_layouts)
    : mix_presentation_id_to_audio_objects_and_metadata_(
          mix_presentation_id_to_audio_objects_and_metadata),
      audio_object_to_audio_element_(audio_object_to_audio_element),
      num_samples_per_frame_(num_samples_per_frame),
      input_layouts_(input_layouts),
      mix_presentation_handler_(samples_per_sec,
                                audio_object_to_audio_element) {}

absl::StatusOr<IAMF> IAMF::Create(const ADM& adm, int32_t max_frame_duration_ms,
                                  uint32_t samples_per_sec) {
  int64_t num_samples_per_frame;
  if (const auto status = ComputeNumSamplesPerFrame(
          max_frame_duration_ms, samples_per_sec, num_samples_per_frame);
      !status.ok()) {
    return status;
  }

  std::vector<IamfInputLayout> input_layouts;
  input_layouts.reserve(adm.audio_objects.size());
  for (auto audio_object : adm.audio_objects) {
    const auto input_layout = LookupInputLayoutFromAudioPackFormatId(
        audio_object.audio_pack_format_id_refs[0]);
    if (!input_layout.ok()) {
      return input_layout.status();
    }

    input_layouts.push_back(*input_layout);
  }

  std::map<int32_t, AudioObjectsAndMetadata>
      mix_presentation_id_to_audio_objects_and_metadata;
  std::map<std::string, uint32_t> audio_object_to_audio_element;
  GenerateAudioObjectsMap(adm,
                          mix_presentation_id_to_audio_objects_and_metadata,
                          audio_object_to_audio_element);

  return IAMF(mix_presentation_id_to_audio_objects_and_metadata,
              audio_object_to_audio_element, num_samples_per_frame,
              samples_per_sec, input_layouts);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
