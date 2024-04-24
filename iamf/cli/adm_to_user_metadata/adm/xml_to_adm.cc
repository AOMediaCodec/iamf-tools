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
#include <cstdint>
#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
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

// It defines adm elements.
enum AdmElement {
  kAudioProgramme = 0,
  kAudioContent = 1,
  kAudioObject = 2,
  kElementDefault = 3
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
void SetHandlerTag(absl::string_view name, Handler& handler) {
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

// Validate an audio object based on the given audio_pack_id. The function
// checks whether the audio object conforms to common definitions and IAMF
// specifications. If not, the audio object is marked as invalid.
void ValidateAudioObject(absl::string_view audio_pack_id, Handler& handler) {
  constexpr absl::string_view kTypeDefinitionDirectSpeakers = "0001";
  constexpr absl::string_view kTypeDefinitionHOA = "0004";
  constexpr absl::string_view kTypeDefinitionBinaural = "0005";

  // Check if the type_definition = DirectSpeakers/HOA/Binaural.
  absl::string_view type_definition = audio_pack_id.substr(3, 4);
  absl::string_view audio_pack_id_yyyy_part = audio_pack_id.substr(7, 4);

  absl::Status status = absl::OkStatus();
  // Check if the metadata belongs to the common definitions (Recommendation
  // ITU-R BS.2094)
  if (IsUserMetadataDefined(audio_pack_id_yyyy_part)) {
    status = absl::InvalidArgumentError("Not under common definition.");
  } else if (type_definition == kTypeDefinitionDirectSpeakers) {
    if (!IsLoudspeakerLayoutValid(audio_pack_id_yyyy_part)) {
      status = absl::InvalidArgumentError(
          "Loudspeaker layout is not supported by IAMF");
    }
  } else if (type_definition == kTypeDefinitionHOA) {
    if (!IsHoaLayoutValid(audio_pack_id_yyyy_part)) {
      status = absl::InvalidArgumentError("HOA layout is not known");
    }
  } else if (type_definition == kTypeDefinitionBinaural) {
    if (!IsBinauralLayoutValid(audio_pack_id_yyyy_part)) {
      status = absl::InvalidArgumentError("Binaural layout is not known.");
    }
  } else {
    status = absl::InvalidArgumentError(
        absl::StrCat("Unsupported type_definition= ", type_definition));
  }

  if (!status.ok()) {
    LOG(WARNING) << "Ignoring unknown object with audio_object_id= "
                 << handler.audio_object_id
                 << ". audio_pack_id= " << audio_pack_id
                 << ". Error: " << status;
    handler.invalid_audio_objects.insert(handler.audio_object_id);
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
          std::cout << "Unexpected case" << std::endl;
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
          std::cout << "Unexpected case" << std::endl;
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
          // Validate audio object based on audio pack id.
          absl::string_view audio_pack_id(text, len);
          ValidateAudioObject(audio_pack_id, handler);
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
          std::cout << "Unexpected case" << std::endl;
        }
      }
      // To handle unwanted character like spaces, new lines.
      handler.audio_object_tag = kObjectDefault;
      break;
    }
    case kElementDefault: {
      break;
    }
    default: {
      std::cout << "Unexpected case" << std::endl;
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
  } else {
    SetHandlerTag(adm_element, handler);
  }
}

}  // namespace

absl::StatusOr<ADM> ParseXmlToAdm(absl::string_view xml_data,
                                  int32_t importance_threshold) {
  Handler handler;

  // Creating an XML parser and attaching a handler object to it. Also, parser
  // is linked with functions that have logic to deal with the start tag of XML
  // and the character of XML.
  XML_Parser parser = XML_ParserCreate(nullptr);
  XML_SetUserData(parser, &handler);
  XML_SetStartElementHandler(parser, XMLStartTagHandlerForExpat);
  XML_SetCharacterDataHandler(parser, XMLCharacterDataHandlerForExpat);

  switch (const auto xml_status =
              XML_Parse(parser, xml_data.data(), xml_data.length(), true)) {
    case XML_STATUS_OK:
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
