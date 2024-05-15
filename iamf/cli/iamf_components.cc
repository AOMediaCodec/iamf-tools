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
#include "iamf/cli/iamf_components.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/cli/mix_presentation_finalizer.h"
#include "iamf/cli/obu_sequencer.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {

std::unique_ptr<MixPresentationFinalizerBase> CreateMixPresentationFinalizer(
    const std::string& /*file_name_prefix*/,
    std::optional<uint8_t> /*output_wav_file_bit_depth_override*/,
    bool /*validate_loudness*/) {
  return std::make_unique<
      MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizer>();
}

std::vector<std::unique_ptr<ObuSequencerBase>> CreateObuSequencers(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::string& output_iamf_directory,
    const bool include_temporal_delimiters) {
  const auto leb_generator = LebGenerator::Create(user_metadata);
  if (leb_generator == nullptr) {
    LOG(ERROR) << "Failed to create LebGenerator.";
    return {};
  }

  std::vector<std::unique_ptr<ObuSequencerBase>> obu_sequencers;
  const std::string& prefix =
      user_metadata.test_vector_metadata().file_name_prefix();

  // Create an OBU sequencer that writes to a standalone IAMF file.
  // TODO(b/314895932): Find a more portable alternative to `/dev/null/`.
  const std::string iamf_filename =
      prefix.empty() ? "/dev/null"
                     : std::filesystem::path(output_iamf_directory) /
                           std::filesystem::path(absl::StrCat(prefix, ".iamf"));
  obu_sequencers.emplace_back(std::make_unique<ObuSequencerIamf>(
      iamf_filename, include_temporal_delimiters, *leb_generator));

  return obu_sequencers;
}

}  // namespace iamf_tools
