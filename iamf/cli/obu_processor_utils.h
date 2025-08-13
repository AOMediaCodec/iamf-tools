#ifndef CLI_OBU_PROCESSOR_UTILS_H_
#define CLI_OBU_PROCESSOR_UTILS_H_

#include <cstdint>
#include <list>
#include <optional>

#include "absl/status/statusor.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A selected Mix and Layout indices found in the Mixes. */
struct SelectedMixPresentation {
  DecodedUleb128 mix_presentation_id;
  Layout output_layout;
  // The index of the sub-mix where the selected Layout was found.
  int sub_mix_index;
  // The index of the selected Layout of possible sub-mix Layouts.
  int layout_index;
};

/*!\brief Finds a MixPresentation/Layout given optional ID/Layout.
 *
 * If the ID is specified and found, we use that Mix Presentation.
 * Otherwise we use the MixPresentation matching the given Layout.
 * If neither ID nor Layout is specified, we default to the first.
 *
 * If the selected Mix (found by ID, layout or default first) does
 * not contain the desired Layout, we will push back a new Layout.
 *
 * \param supported_mix_presentations All usable MixPresentations.
 * \param desired_layout If specified, the decoding target Layout.
 * \param desired_mix_presentation_id The target Mix if specified.
 * \returns If no error, returns Mix Presentation, ID, and Layout.
 */
absl::StatusOr<SelectedMixPresentation> FindMixPresentationAndLayout(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const std::optional<Layout>& desired_layout,
    std::optional<uint32_t> desired_mix_presentation_id);

}  // namespace iamf_tools

#endif  // CLI_OBU_PROCESSOR_UTILS_H_
