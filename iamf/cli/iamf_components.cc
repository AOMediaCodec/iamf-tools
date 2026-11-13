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

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/itu_1770_4/loudness_calculator_factory_itu_1770_4.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/obu_sequencer_iamf.h"
#include "iamf/common/leb_generator.h"

namespace iamf_tools {

std::unique_ptr<LoudnessCalculatorFactoryBase>
CreateLoudnessCalculatorFactory() {
  return std::make_unique<LoudnessCalculatorFactoryItu1770_4>();
}

namespace {
constexpr absl::string_view kOmitIamfFile = "";
}  // namespace

std::vector<std::unique_ptr<ObuSequencerBase>> CreateObuSequencers(
    absl::string_view file_name_prefix,
    const std::string& output_iamf_directory, bool include_temporal_delimiters,
    const LebGenerator& leb_generator) {
  std::vector<std::unique_ptr<ObuSequencerBase>> obu_sequencers;

  // Create an OBU sequencer that writes to a standalone IAMF file.
  const std::string iamf_filename =
      file_name_prefix.empty()
          ? std::string(kOmitIamfFile)
          : (std::filesystem::path(output_iamf_directory) /
             std::filesystem::path(absl::StrCat(file_name_prefix, ".iamf")))
                .string();
  obu_sequencers.emplace_back(std::make_unique<ObuSequencerIamf>(
      iamf_filename, include_temporal_delimiters, leb_generator));

  return obu_sequencers;
}

}  // namespace iamf_tools
