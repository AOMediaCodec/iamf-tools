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

#ifndef CLI_DOWNMIXER_FACTORY_H_
#define CLI_DOWNMIXER_FACTORY_H_

#include <list>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/downmixer.h"

namespace iamf_tools {

/*!\brief Factory for creating downmixers.
 *
 * Assembles a sequence of down-mixers to transform the requested input layout
 * into the required output substream layouts.
 */
class DownmixerFactory {
 public:
  /*!\brief Creates a list of down-mixers for the given channel labels.
   *
   * Creates spatial down-mixers (e.g. 7.1.4 to 5.1.2) needed based on input
   * labels and the output substream requirements.
   *
   * \param labels_to_downmix Input channel labels to down-mix.
   * \param substream_id_to_labels Mapping of substream IDs to their labels.
   * \return List of DownMixer functions to apply in sequence, or a specific
   *         status on failure.
   */
  static absl::StatusOr<std::list<DownMixer>> CreateScalableChannelDownmixers(
      const absl::flat_hash_set<ChannelLabel::Label>& labels_to_downmix,
      const SubstreamIdLabelsMap& substream_id_to_labels);
};

}  // namespace iamf_tools

#endif  // CLI_DOWNMIXER_FACTORY_H_
