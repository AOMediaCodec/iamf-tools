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

#include "iamf/cli/temporal_unit_processor.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<TemporalUnitProcessor> TemporalUnitProcessor::Create(
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    ReadBitBuffer* absl_nonnull read_bit_buffer) {
  RETURN_IF_NOT_OK(ValidateNotNull(read_bit_buffer, "read_bit_buffer"));

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements, mix_presentation_obus, param_definition_variants));

  absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element;
  absl::flat_hash_set<DecodedUleb128> audio_element_ids;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    audio_element_ids.insert(audio_element_id);
    for (const auto& [substream_id, unused_labels] :
         audio_element_with_data.substream_id_to_labels) {
      auto [unused_iter, inserted] = substream_id_to_audio_element.insert(
          {substream_id, &audio_element_with_data});
      if (!inserted) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Duplicated substream ID: ", substream_id,
            " associated with audio element ID: ", audio_element_id));
      }
    }
  }

  std::unique_ptr<GlobalTimingModule> global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definition_variants);
  if (global_timing_module == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }

  auto parameters_manager = ParametersManager::Create(audio_elements);
  if (!parameters_manager.ok()) {
    return parameters_manager.status();
  }
  ABSL_CHECK_NE(*parameters_manager, nullptr);

  return TemporalUnitProcessor(std::move(param_definition_variants),
                               std::move(substream_id_to_audio_element),
                               std::move(audio_element_ids),
                               std::move(global_timing_module),
                               *std::move(parameters_manager), read_bit_buffer);
}

absl::Status TemporalUnitProcessor::ProcessTemporalUnit(
    bool /*eos_is_end_of_sequence*/,
    std::optional<OutputTemporalUnit>& /*output_temporal_unit*/,
    bool& /*continue_processing*/) {
  return absl::UnimplementedError(
      "ProcessTemporalUnit is not yet implemented.");
}

}  // namespace iamf_tools
