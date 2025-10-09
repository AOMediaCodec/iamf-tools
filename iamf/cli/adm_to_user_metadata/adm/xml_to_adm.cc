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

#include "iamf/cli/adm_to_user_metadata/adm/xml_to_adm.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "expat/lib/expat.h"
#include "expat/lib/expat_external.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {
constexpr absl::string_view kTypeDefinitionDirectSpeakers = "0001";
constexpr absl::string_view kTypeDefinitionObject = "0003";
constexpr absl::string_view kTypeDefinitionHOA = "0004";
constexpr absl::string_view kTypeDefinitionBinaural = "0005";

// It defines adm elements.
enum AdmElement {
  kAudioProgramme = 0,
  kAudioContent = 1,
  kAudioObject = 2,
  kAudioPack = 3,
  kAudioChannel = 4,
  kAudioBlock = 5,
  kElementDefault = 6
};

// It defines the attributes of audio programme.
enum AdmProgrammeElement {
  kAudioContentIDRef = 0,
  kIntegratedLoudness = 1,
  kMaxTruePeak = 2,
  kDialogueLoudness = 3,
  kAudioProgrammeAudioPackFormatIDRef = 4,
  kProgrammeDefault = 5
};

// It defines the attributes of audio content.
enum AdmContentElement { kAudioObjectIDRef = 0, kContentDefault = 1 };

// It defines the attributes of audio object.
enum AdmObjectElement {
  kAudioObjectAudioPackFormatIDRef = 0,
  kAudioTrackUIDRef = 1,
  kAudioComplementaryObjectIDRef = 2,
  kGain = 3,
  kAudioObjectLabel = 4,
  kObjectDefault = 5
};

// It defines the attributes of audio pack format.
enum AdmPackFormat {
  kAudioPackAudioChannelFormatIDRef = 0,
  kAudioPackLabel = 1,
  kPackDefault = 2
};

// It defines the attributes of audio channel format.
enum AdmChannelFormat { kAudioChannelLabel = 0, kChannelDefault = 1 };

// It defines the attributes of audio block.
enum AdmBlockFormat {
  kX = 0,
  kY = 1,
  kZ = 2,
  kAudioBlockLabel = 3,
  kBlockDefault = 4
};

// This class is used by xml parser to collect and store various attributes and
// information of xml.
struct Handler {
  ADM adm;
  absl::flat_hash_set<std::string> invalid_audio_objects;
  std::string audio_object_id;

  AdmElement parent = kElementDefault;
  AdmProgrammeElement audio_programme_tag = kProgrammeDefault;
  AdmContentElement audio_content_tag = kContentDefault;
  AdmObjectElement audio_object_tag = kObjectDefault;
  AdmPackFormat audio_pack_tag = kPackDefault;
  AdmChannelFormat audio_channel_tag = kChannelDefault;
  AdmBlockFormat audio_block_tag = kBlockDefault;

  absl::Status status = absl::OkStatus();
};

void UpdateErrorStatusIfFalse(bool status, absl::string_view field_name,
                              Handler& handler) {
  if (!status && handler.status.ok()) {
    handler.status = absl::InvalidArgumentError(
        absl::StrCat("Failed to parse ", field_name));
  }
}

// This function sets the handler's tag for program, content, or object based
// upon the name attribute.
void SetHandlerTag(absl::string_view name, const char** atts,
                   Handler& handler) {
  if (name == "audioContentIDRef") {
    handler.audio_programme_tag = kAudioContentIDRef;
  } else if (name == "integratedLoudness") {
    handler.audio_programme_tag = kIntegratedLoudness;
  } else if (name == "maxTruePeak") {
    handler.audio_programme_tag = kMaxTruePeak;
  } else if (name == "dialogueLoudness") {
    handler.audio_programme_tag = kDialogueLoudness;
  } else if (name == "audioObjectIDRef") {
    handler.audio_content_tag = kAudioObjectIDRef;
  } else if (name == "audioPackFormatIDRef") {
    if (handler.parent == kAudioProgramme) {
      handler.audio_programme_tag = kAudioProgrammeAudioPackFormatIDRef;
    } else {
      handler.audio_object_tag = kAudioObjectAudioPackFormatIDRef;
    }
  } else if (name == "audioTrackUIDRef") {
    handler.audio_object_tag = kAudioTrackUIDRef;
  } else if (name == "audioComplementaryObjectIDRef") {
    handler.audio_object_tag = kAudioComplementaryObjectIDRef;
  } else if (name == "gain") {
    handler.audio_object_tag = kGain;
  } else if (name == "audioObjectLabel") {
    handler.audio_object_tag = kAudioObjectLabel;
  } else if (name == "audioPackLabel") {
    handler.audio_pack_tag = kAudioPackLabel;
  } else if (name == "audioChannelFormatIDRef") {
    handler.audio_pack_tag = kAudioPackAudioChannelFormatIDRef;
  } else if (name == "position") {
    handler.audio_block_tag = kBlockDefault;
    for (int32_t i = 0; atts[i]; i += 2) {
      if ((std::string)atts[i + 1] == "X") {
        handler.audio_block_tag = kX;
      } else if ((std::string)atts[i + 1] == "Y") {
        handler.audio_block_tag = kY;
      } else if ((std::string)atts[i + 1] == "Z") {
        handler.audio_block_tag = kZ;
      }
    }
  } else if (name == "audioBlockFormatID") {
    handler.audio_block_tag = kAudioBlockLabel;
  }
}

// Sets the attributes of AudioProgramme.
void SetAudioProgrammeValue(absl::string_view key, absl::string_view value,
                            AudioProgramme& audio_programme) {
  if (key == "audioProgrammeID") {
    audio_programme.id = value;
  } else if (key == "audioProgrammeName") {
    audio_programme.name = value;
  } else if (key == "audioProgrammeLabel") {
    audio_programme.audio_programme_label = value;
  }
}

// Sets the attributes of AudioContent.
void SetAudioContentValue(absl::string_view key, absl::string_view value,
                          AudioContent& audio_content) {
  if (key == "audioContentID") {
    audio_content.id = value;
  } else if (key == "audioContentName") {
    audio_content.name = value;
  }
}

// Sets the attributes of AudioObject.
void SetAudioObjectValue(absl::string_view key, absl::string_view value,
                         AudioObject& audio_object, Handler& handler) {
  if (key == "audioObjectID") {
    handler.audio_object_id = value;
    audio_object.id = value;
  } else if (key == "audioObjectName") {
    audio_object.name = value;
  } else if (key == "importance") {
    UpdateErrorStatusIfFalse(absl::SimpleAtoi(value, &audio_object.importance),
                             "importance", handler);
  }
}

// Sets the attributes of AudioPack.
void SetAudioPackValue(absl::string_view key, absl::string_view value,
                       AudioPackFormat& audio_pack) {
  if (key == "audioPackFormatID") {
    audio_pack.id = (std::string)value;
  } else if (key == "audioPackFormatName") {
    audio_pack.name = (std::string)value;
  } else if (key == "typeLabel") {
    audio_pack.audio_pack_label = (std::string)value;
  }
}

// Sets the attributes of AudioChannel.
void SetAudioChannelValue(absl::string_view key, absl::string_view value,
                          AudioChannelFormat& audio_channel) {
  if (key == "audioChannelFormatID") {
    audio_channel.id = (std::string)value;
  } else if (key == "audioChannelFormatName") {
    audio_channel.name = (std::string)value;
  } else if (key == "typeLabel") {
    audio_channel.audio_channel_label = (std::string)value;
  }
}

// Parse and store the timing information in Audio Block.
// The input string which holds the timing information will be in the format
// 'hh:mm:ss.zzzzz'.
void ParseTimingInfo(absl::string_view time_string, BlockTime& time) {
  time.hour = std::stoi(std::string(time_string.substr(0, 2)));
  time.minute = std::stoi(std::string(time_string.substr(3, 2)));
  time.second = std::stod(std::string(time_string.substr(6)));
}

// Sets the attributes of AudioBlock.
void SetAudioBlockValue(absl::string_view key, absl::string_view value,
                        AudioBlockFormat& audio_block) {
  if (key == "audioBlockFormatID") {
    audio_block.id = value;
  } else if (key == "rtime") {
    ParseTimingInfo(value, audio_block.rtime);
  } else if (key == "duration") {
    ParseTimingInfo(value, audio_block.duration);
  }
}

// Removes objects from the ADM structure based on the given importance
// threshold. Also, removes audio objects with IDs found in the set of invalid
// audio objects.
void RemoveLowImportanceAndInvalidAudioObjects(int32_t importance_threshold,
                                               Handler& handler) {
  std::vector<AudioObject>& audio_object_list = handler.adm.audio_objects;
  audio_object_list.erase(
      std::remove_if(audio_object_list.begin(), audio_object_list.end(),
                     [&](AudioObject value) {
                       return value.importance < importance_threshold ||
                              (handler.invalid_audio_objects.find(value.id) !=
                               handler.invalid_audio_objects.end());
                     }),
      audio_object_list.end());
}

// Checks if the metadata is user defined or part of the common definitions.
// NOTE: An ADM audioPackFormatID AP_yyyyxxxx which belongs to common
// definitions would have 'xxxx' in the range [0x0001, 0x0FFF].
bool IsUserMetadataDefined(absl::string_view xxxx_substring) {
  std::istringstream iss(std::string(xxxx_substring), std::ios_base::in);
  int32_t int_value;
  iss >> std::hex >> int_value;
  return int_value > 0x0fff;
}

// Validates the specific layout in terms of the 'xxxx' digits of
// audioPackFormatId (AP_yyyyxxxx) in ADM.
bool IsLoudspeakerLayoutValid(absl::string_view xxxx_substring) {
  static const absl::NoDestructor<absl::flat_hash_set<std::string>>
      kValidLoudspeakerLayouts({
          {"0001"},  // Mono
          {"0002"},  // Stereo
          {"0003"},  // 5.1
          {"0004"},  // 5.1.2
          {"0005"},  // 5.1.4
          {"000f"},  // 7.1
          {"0017"},  // 7.1.4
      });
  return kValidLoudspeakerLayouts->contains(xxxx_substring);
}

// Validates the HOA layout in terms of the 'xxxx' digits of audioPackFormatId
// (AP_yyyyxxxx) in ADM.
bool IsHoaLayoutValid(absl::string_view xxxx_substring) {
  static const absl::NoDestructor<absl::flat_hash_set<std::string>>
      kValidHoaLayouts({
          {"0001"},  // First-order ambisonics.
          {"0002"},  // Second-order ambisonics.
          {"0003"},  // Third-order ambisonics.
      });
  return kValidHoaLayouts->contains(xxxx_substring);
}

// Validates the Binaural layout in terms of the 'xxxx' digits of
// audioPackFormatId (AP_yyyyxxxx) in ADM.
bool IsBinauralLayoutValid(absl::string_view xxxx_substring) {
  constexpr absl::string_view kValidBinauralLayout = "0001";
  return xxxx_substring == kValidBinauralLayout;
}

// Converts channel names to their abbreviated channel codes and creates an
// audio pack layout string of channel codes separated by commas.
absl::StatusOr<std::string> CreatePackLayout(
    const std::vector<std::string>& channel_names) {
  static const std::unordered_map<std::string, std::string> channel_name_map = {
      {"RoomCentricLeft", "L"},
      {"RoomCentricRight", "R"},
      {"RoomCentricCenter", "C"},
      {"RoomCentricLFE", "LFE"},
      {"RoomCentricLeftSideSurround", "Lss"},
      {"RoomCentricRightSideSurround", "Rss"},
      {"RoomCentricLeftRearSurround", "Lrs"},
      {"RoomCentricRightRearSurround", "Rrs"},
      {"RoomCentricLeftTopSurround", "Lts"},
      {"RoomCentricRightTopSurround", "Rts"},
      {"RoomCentricLeftSurround", "Ls"},
      {"RoomCentricRightSurround", "Rs"}};

  std::string pack_layout = "";
  for (const auto& channel_name : channel_names) {
    auto channel_name_iter = channel_name_map.find(channel_name);
    if (channel_name_iter != channel_name_map.end()) {
      pack_layout += channel_name_iter->second;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid channel format= ", channel_name));
    }
    pack_layout += ",";
  }

  if (!pack_layout.empty()) {
    pack_layout.pop_back();
  }
  return pack_layout;
}

// Determines whether the given audio pack layout string exists within known
// valid pack layouts. Returns an error if invalid.
absl::Status ValidatePackLayout(const std::string& pack_layout) {
  static const absl::NoDestructor<absl::flat_hash_set<std::string>>
      kValidPackLayouts({
          {"L,R"},
          {"L,R,C"},
          {"L,R,C,Ls,Rs"},
          {"L,R,C,LFE,Ls,Rs"},
          {"L,R,C,Lss,Rss,Lrs,Rrs"},
          {"L,R,C,LFE,Lss,Rss,Lrs,Rrs"},
          {"L,R,C,Lss,Rss,Lrs,Rrs,Lts,Rts"},
          {"L,R,C,LFE,Lss,Rss,Lrs,Rrs,Lts,Rts"},
      });

  if (!kValidPackLayouts->contains(pack_layout)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid pack layout= ", pack_layout));
  }

  return absl::OkStatus();
}

// Check if the metadata belongs to the common definitions (Recommendation ITU-R
// BS.2094)
absl::Status ValidateAdmObjectForDefaultAdm(
    absl::string_view type_definition,
    absl::string_view audio_pack_id_yyyy_part) {
  if (IsUserMetadataDefined(audio_pack_id_yyyy_part)) {
    return absl::InvalidArgumentError("Not under common definition.");
  }

  if (type_definition == kTypeDefinitionDirectSpeakers) {
    if (!IsLoudspeakerLayoutValid(audio_pack_id_yyyy_part)) {
      return absl::InvalidArgumentError(
          "Loudspeaker layout is not supported by IAMF");
    }
  } else if (type_definition == kTypeDefinitionHOA) {
    if (!IsHoaLayoutValid(audio_pack_id_yyyy_part)) {
      return absl::InvalidArgumentError("HOA layout is not known");
    }
  } else if (type_definition == kTypeDefinitionBinaural) {
    if (!IsBinauralLayoutValid(audio_pack_id_yyyy_part)) {
      return absl::InvalidArgumentError("Binaural layout is not known.");
    }
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported type_definition= ", type_definition));
  }

  return absl::OkStatus();
}

absl::Status ValidateAdmObjectForDolbyAdm(const ADM& adm,
                                          const AudioObject& audio_object,
                                          absl::string_view type_definition) {
  if (type_definition != kTypeDefinitionDirectSpeakers &&
      type_definition != kTypeDefinitionObject) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported type_definition= ", type_definition,
                     " when processing a Dolby ADM."));
  }
  if (audio_object.audio_pack_format_id_refs.size() != 1) {
    return absl::InvalidArgumentError(
        "Expected only one audio pack ID ref for an audio object in a Dolby "
        "ADM file.");
  }

  absl::string_view audio_pack_id = audio_object.audio_pack_format_id_refs[0];
  auto pack_id = std::find_if(adm.audio_packs.begin(), adm.audio_packs.end(),
                              [&audio_pack_id](const AudioPackFormat& pack) {
                                return pack.id == audio_pack_id;
                              });
  size_t pack_index = (pack_id != adm.audio_packs.end())
                          ? std::distance(adm.audio_packs.begin(), pack_id)
                          : 0;

  auto num_channels_in_pack =
      adm.audio_packs[pack_index].audio_channel_format_id_refs_map.size();
  auto num_tracks_in_object = audio_object.audio_track_uid_ref.size();
  if (type_definition == kTypeDefinitionObject) {
    if (num_tracks_in_object != 1) {
      return absl::InvalidArgumentError(
          "Audio object should have only 1 track ID ref for type definition "
          "object");
    }
    if (num_channels_in_pack != 1) {
      return absl::InvalidArgumentError(
          "Audio pack should have only 1 channel ID ref for type definition "
          "object");
    }
  } else {
    ABSL_CHECK_EQ(type_definition, kTypeDefinitionDirectSpeakers);
    if (num_tracks_in_object > 10) {
      return absl::InvalidArgumentError(
          "Maximum number of occurrences of track UID refs for DirectSpeakers "
          "is 10.");
    }
    if (num_channels_in_pack > 10) {
      return absl::InvalidArgumentError(
          "Maximum number of occurrences of channel ID refs for DirectSpeakers "
          "is 10.");
    }

    // Create an audio pack layout string based on channel names present within
    // an audio pack.
    std::vector<std::string> channel_names;
    for (auto& channel_ref :
         adm.audio_packs[pack_index].audio_channel_format_id_refs_map) {
      auto& audio_channel = adm.audio_channels[channel_ref.second];
      channel_names.push_back(audio_channel.name);
    }

    // Validate audio pack layout.
    auto audio_pack_layout = CreatePackLayout(channel_names);
    if (!audio_pack_layout.ok()) {
      return audio_pack_layout.status();
    }
    return ValidatePackLayout(*audio_pack_layout);
  }

  return absl::OkStatus();
}

// Validates audio objects based on the input file type.
void ValidateAudioObjects(const ADM& adm, Handler& handler) {
  absl::Status status = absl::OkStatus();
  std::vector<std::string> audio_pack_layouts;

  for (auto& audio_object : adm.audio_objects) {
    if (audio_object.audio_pack_format_id_refs.empty()) {
      // Skip the empty audio objects.
      continue;
    }

    absl::string_view audio_pack_id = audio_object.audio_pack_format_id_refs[0];
    absl::string_view type_definition = audio_pack_id.substr(3, 4);
    absl::string_view audio_pack_id_yyyy_part = audio_pack_id.substr(7, 4);
    if (adm.file_type == kAdmFileTypeDefault) {
      status = ValidateAdmObjectForDefaultAdm(type_definition,
                                              audio_pack_id_yyyy_part);
    } else {
      ABSL_CHECK_EQ(adm.file_type, kAdmFileTypeDolby);
      status = ValidateAdmObjectForDolbyAdm(adm, audio_object, type_definition);
    }
    if (!status.ok()) {
      ABSL_LOG(WARNING) << "Ignoring unknown object with audio_object_id= "
                        << audio_object.id << ". Error: " << status;
      handler.invalid_audio_objects.insert(audio_object.id);
    }
  }
}

//  A function to use with `expat::XML_SetCharacterDataHandler`. This function
//  is responsible for storing character data encountered while parsing an AXML
//  chunk in their respective handler.adm class attributes.
void XMLCharacterDataHandlerForExpat(void* parser_data, const XML_Char* text,
                                     int32_t len) {
  Handler& handler = *static_cast<Handler*>(parser_data);

  int32_t idx = 0;
  switch (handler.parent) {
    case kAudioProgramme: {
      // Populates audio programme class.
      idx = handler.adm.audio_programmes.size();
      auto& loudness_metadata =
          handler.adm.audio_programmes[idx - 1].loudness_metadata;
      ReferenceLayout reference_layout;
      switch (handler.audio_programme_tag) {
        case kAudioContentIDRef: {
          handler.adm.audio_programmes[idx - 1].audio_content_id_refs.push_back(
              std::string(text, len));
          break;
        }
        case kIntegratedLoudness: {
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &loudness_metadata.integrated_loudness),
              "integrated_loudness", handler);
          break;
        }
        case kMaxTruePeak: {
          // Activate the optional then read it in.
          loudness_metadata.max_true_peak = 0.0;
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &loudness_metadata.max_true_peak.value()),
              "max_true_peak", handler);
          break;
        }
        case kDialogueLoudness: {
          // Activate the optional then read it in.
          loudness_metadata.dialogue_loudness = 0.0;
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &loudness_metadata.dialogue_loudness.value()),
              "dialogue_loudness", handler);

          break;
        }
        case kAudioProgrammeAudioPackFormatIDRef: {
          reference_layout.audio_pack_format_id_ref.push_back(
              std::string(text, len));
          handler.adm.audio_programmes[idx - 1]
              .authoring_information.reference_layout = reference_layout;
          break;
        }
        case kProgrammeDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }
      // To handle unwanted character like spaces, new lines.
      handler.audio_programme_tag = kProgrammeDefault;
      break;
    }
    case kAudioContent: {
      // Populates audio content class.
      idx = handler.adm.audio_contents.size();
      switch (handler.audio_content_tag) {
        case kAudioObjectIDRef: {
          handler.adm.audio_contents[idx - 1].audio_object_id_ref.push_back(
              std::string(text, len));
          break;
        }
        case kContentDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }
      // To handle unwanted character like spaces, new lines.
      handler.audio_content_tag = kContentDefault;
      break;
    }
    case kAudioObject: {
      // Populates audio object class.
      idx = handler.adm.audio_objects.size();
      switch (handler.audio_object_tag) {
        case kAudioObjectAudioPackFormatIDRef: {
          handler.adm.audio_objects[idx - 1]
              .audio_pack_format_id_refs.push_back(std::string(text, len));
          break;
        }
        case kAudioTrackUIDRef: {
          handler.adm.audio_objects[idx - 1].audio_track_uid_ref.push_back(
              std::string(text, len));
          break;
        }
        case kAudioComplementaryObjectIDRef: {
          handler.adm.audio_objects[idx - 1]
              .audio_comple_object_id_ref.push_back(std::string(text, len));
          break;
        }
        case kGain: {
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &handler.adm.audio_objects[idx - 1].gain),
              "gain", handler);
          break;
        }
        case kAudioObjectLabel: {
          handler.adm.audio_objects[idx - 1].audio_object_label =
              (std::string(text, len));
          break;
        }
        case kObjectDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }
      // To handle unwanted character like spaces, new lines.
      handler.audio_object_tag = kObjectDefault;
      break;
    }
    case kAudioPack: {
      // Populates audio pack object.
      idx = handler.adm.audio_packs.size();
      switch (handler.audio_pack_tag) {
        case kAudioPackAudioChannelFormatIDRef: {
          handler.adm.audio_packs[idx - 1]
              .audio_channel_format_id_refs_map.emplace_back(
                  std::string(text, len), size_t(-1));
          break;
        }
        case kAudioPackLabel: {
          handler.adm.audio_packs[idx - 1].id = (std::string(text, len));
          break;
        }
        case kPackDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }

      // To handle unwanted character like spaces, new lines.
      handler.audio_pack_tag = kPackDefault;
      break;
    }
    case kAudioChannel: {
      // Populates audio channel object.
      idx = handler.adm.audio_channels.size();
      switch (handler.audio_channel_tag) {
        case kAudioChannelLabel: {
          handler.adm.audio_channels[idx - 1].id = (std::string(text, len));
          break;
        }
        case kChannelDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }

      // To handle unwanted character like spaces, new lines.
      handler.audio_channel_tag = kChannelDefault;
      break;
    }
    case kAudioBlock: {
      // Populates audio block object.
      idx = handler.adm.audio_channels.size();
      auto& audio_blocks = handler.adm.audio_channels[idx - 1].audio_blocks;
      switch (handler.audio_block_tag) {
        case kX: {
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &audio_blocks.back().position.x),
              "position", handler);
          break;
        }
        case kY: {
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &audio_blocks.back().position.y),
              "position", handler);
          break;
        }
        case kZ: {
          UpdateErrorStatusIfFalse(
              absl::SimpleAtof(absl::string_view(text, len),
                               &audio_blocks.back().position.z),
              "position", handler);
          break;
        }
        case kAudioBlockLabel: {
          audio_blocks.back().id = (std::string(text, len));
          break;
        }
        case kBlockDefault: {
          break;
        }
        default: {
          ABSL_LOG(ERROR) << "Unexpected case";
        }
      }

      // To handle unwanted characters like spaces, new lines.
      handler.audio_block_tag = kBlockDefault;
      break;
    }
    case kElementDefault: {
      break;
    }
    default: {
      ABSL_LOG(ERROR) << "Unexpected case";
    }
  }
}

// A function to use with `expat::XML_SetStartElementHandler`. It sets the
// handler's parent tag depending on the name attribute.
void XMLStartTagHandlerForExpat(void* parser_data, const char* name,
                                const char** atts) {
  Handler& handler = *static_cast<Handler*>(parser_data);
  absl::string_view adm_element(name);
  if (adm_element == "audioProgramme") {
    // If the tag 'audioProgramme' is encountered while parsing the axml, create
    // an instance of AudioProgramme class, populate its attributes and add it
    // to ADM.
    handler.parent = kAudioProgramme;
    AudioProgramme audio_programme;
    LoudnessMetadata loudness_metadata;
    AuthoringInformation authoring_information;
    audio_programme.loudness_metadata = loudness_metadata;
    audio_programme.authoring_information = authoring_information;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioProgrammeValue(absl::string_view(atts[i]),
                             absl::string_view(atts[i + 1]), audio_programme);
    }
    handler.adm.audio_programmes.push_back(audio_programme);
  } else if (adm_element == "audioContent") {
    // If the tag 'audioContent' is encountered while parsing the axml, create
    // an instance of AudioContent class, populate its attributes and add it to
    // ADM.
    handler.parent = kAudioContent;
    AudioContent audio_content;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioContentValue(absl::string_view(atts[i]),
                           absl::string_view(atts[i + 1]), audio_content);
    }
    handler.adm.audio_contents.push_back(audio_content);
  } else if (adm_element == "audioObject") {
    // If the tag 'audioObject' is encountered while parsing the axml, create an
    // instance of AudioObject class, populate its attributes and add it to ADM.
    handler.parent = kAudioObject;
    AudioObject audio_object;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioObjectValue(absl::string_view(atts[i]),
                          absl::string_view(atts[i + 1]), audio_object,
                          handler);
    }
    handler.adm.audio_objects.push_back(audio_object);
  } else if (adm_element == "audioPackFormat") {
    // If the tag 'audioPackFormat' is encountered while parsing the axml,
    // create an instance of AudioPack class, populate its attributes and add it
    // to ADM.
    handler.parent = kAudioPack;
    AudioPackFormat audio_pack;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioPackValue(absl::string_view(atts[i]),
                        absl::string_view(atts[i + 1]), audio_pack);
    }
    handler.adm.audio_packs.push_back(audio_pack);
  } else if (adm_element == "audioChannelFormat") {
    // If the tag 'audioChannelFormat' is encountered while parsing the axml,
    // create an instance of AudioChannel class, populate its attributes and add
    // it to ADM.
    handler.parent = kAudioChannel;
    AudioChannelFormat audio_channel;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioChannelValue(absl::string_view(atts[i]),
                           absl::string_view(atts[i + 1]), audio_channel);
    }
    handler.adm.audio_channels.push_back(audio_channel);
  } else if (adm_element == "audioBlockFormat") {
    // If the tag 'audioBlockFormat' is encountered while parsing the axml,
    // create an instance of AudioBlockFormat class, populate its attributes and
    // add it to ADM.
    handler.parent = kAudioBlock;
    AudioBlockFormat audio_block;
    CartesianPosition position;
    audio_block.position = position;
    for (int32_t i = 0; atts[i]; i += 2) {
      SetAudioBlockValue(absl::string_view(atts[i]),
                         absl::string_view(atts[i + 1]), audio_block);
    }
    handler.adm.audio_channels.back().audio_blocks.push_back(audio_block);
  } else {
    SetHandlerTag(adm_element, atts, handler);
  }
}

// A function to map each audio pack to their corresponding audio channel
// formats. It sets the corresponding indices into a vector of pairs inside each
// audio pack instance.
void SetChannelIndices(ADM& adm) {
  // Iterate over all audio packs
  for (auto& audio_pack : adm.audio_packs) {
    for (auto& id_ref_and_index : audio_pack.audio_channel_format_id_refs_map) {
      const std::string& channel_id_ref = id_ref_and_index.first;
      auto channel_id =
          std::find_if(adm.audio_channels.begin(), adm.audio_channels.end(),
                       [&channel_id_ref](const AudioChannelFormat& channel) {
                         return channel.id == channel_id_ref;
                       });

      if (channel_id != adm.audio_channels.end()) {
        size_t channel_index =
            std::distance(adm.audio_channels.begin(), channel_id);
        id_ref_and_index.second = channel_index;
      } else {
        ABSL_LOG(WARNING) << "Channel ID ref " << channel_id_ref
                          << " not found!";
      }
    }
  }
}
}  // namespace

absl::StatusOr<ADM> ParseXmlToAdm(absl::string_view xml_data,
                                  int32_t importance_threshold,
                                  AdmFileType file_type) {
  Handler handler;

  // Creating an XML parser and attaching a handler object to it. Also, parser
  // is linked with functions that have logic to deal with the start tag of XML
  // and the character of XML.
  XML_Parser parser = XML_ParserCreate(nullptr);
  XML_SetUserData(parser, &handler);
  handler.adm.file_type = file_type;
  XML_SetStartElementHandler(parser, XMLStartTagHandlerForExpat);
  XML_SetCharacterDataHandler(parser, XMLCharacterDataHandlerForExpat);

  switch (const auto xml_status =
              XML_Parse(parser, xml_data.data(), xml_data.length(), true)) {
    case XML_STATUS_OK:
      SetChannelIndices(handler.adm);
      ValidateAudioObjects(handler.adm, handler);
      RemoveLowImportanceAndInvalidAudioObjects(importance_threshold, handler);
      XML_ParserFree(parser);
      if (!handler.status.ok()) {
        return handler.status;
      }
      return handler.adm;
    case XML_STATUS_ERROR:
      XML_ParserFree(parser);
      return absl::InvalidArgumentError(
          absl::StrCat("XML parsing error. XML_Parse() returned ", xml_status));
    case XML_STATUS_SUSPENDED:
      XML_ParserFree(parser);
      return absl::FailedPreconditionError(absl::StrCat(
          "XML parsing suspended. XML_Parse() returned ", xml_status));
    default:
      XML_ParserFree(parser);
      return absl::UnknownError(absl::StrCat(
          "XML parsing failed. XML_Parse() returned ", xml_status));
  }
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
