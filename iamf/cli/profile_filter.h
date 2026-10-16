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
#ifndef CLI_PROFILE_FILTER_H_
#define CLI_PROFILE_FILTER_H_

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief A class to help filter out profile-specific limiations.
 *
 * This class is intended to be used to determine under which contexts OBUs
 * should be ignored. Properties which are not obviously extension points or
 * currently profile-specific are not considered.
 */
class ProfileFilter {
 public:
  /*!\brief Filter out profiles that should ignore the audio element.
   *
   * \param debugging_context Context to use for error messages.
   * \param audio_element_obu Audio element to filter based on.
   * \param profile_versions Profiles to filter. Unsupported profiles will be
   *        removed from the set.
   * \return `absl::OkStatus` if the audio element is supported by at least one
   *         of the input profile. A specific error otherwise.
   */
  static absl::Status FilterProfilesForAudioElement(
      absl::string_view debugging_context,
      const AudioElementObu& audio_element_obu,
      absl::flat_hash_set<ProfileVersion>& profile_versions);

  /*!\brief Filter out profiles that should ignore the mix presentation.
   *
   * \param audio_elements Audio elements in the IA sequence.
   * \param mix_presentation_obu Mix presentation to filter based on.
   * \param profile_versions Profiles to filter. Unsupported profiles will be
   *        removed from the set.
   * \return `absl::OkStatus` if the mix presentation is supported by at least
   *         one of the input profile. A specific error otherwise.
   */
  static absl::Status FilterProfilesForMixPresentation(
      const DescriptorObus::AudioElementsById& audio_elements,
      const MixPresentationObu& mix_presentation_obu,
      absl::flat_hash_set<ProfileVersion>& profile_versions);

  /*!\brief Validates Mix Presentation OBUs against `primary_profile`.
   *
   * Enforces sequence-level `primary_profile` ordering and composition
   * constraints across `mix_presentation_obus`. Does not check whether
   * individual Mix Presentations are supported by a profile (use
   * `FilterProfilesForMixPresentation` for per-OBU filtering).
   *
   * \param primary_profile Primary profile of the IA Sequence.
   * \param audio_elements Audio elements in the IA sequence.
   * \param mix_presentation_obus Mix presentation OBUs in descriptor order.
   * \return `absl::OkStatus` if the sequence of Mix Presentation OBUs satisfies
   *         the `primary_profile` constraints, or `absl::InvalidArgumentError`
   *         if a constraint is violated.
   */
  static absl::Status ValidateMixPresentationsForPrimaryProfile(
      ProfileVersion primary_profile,
      const DescriptorObus::AudioElementsById& audio_elements,
      const DescriptorObus::MixPresentationObus& mix_presentation_obus);

  /*!\brief Validates Audio Element OBUs against `additional_profile`.
   *
   * Enforces sequence-level `additional_profile` total channel count
   * constraints across all Audio Elements in `audio_elements` recognized by
   * `additional_profile` (IAMF Spec §4.1).
   *
   * \param additional_profile Additional profile of the IA Sequence.
   * \param audio_elements Audio elements in the IA sequence.
   * \return `absl::OkStatus` if the Audio Elements satisfy the
   *         `additional_profile` constraints, or `absl::InvalidArgumentError`
   *         if a constraint is violated.
   */
  static absl::Status ValidateAudioElementsForAdditionalProfile(
      ProfileVersion additional_profile,
      const DescriptorObus::AudioElementsById& audio_elements);
};

}  // namespace iamf_tools

#endif  // CLI_PROFILE_FILTER_H_
