#include "iamf/cli/obu_processor_utils.h"

#include <cstdint>
#include <list>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {
bool MixPresentationContainsLayout(const MixPresentationObu* candidate_mix,
                                   const Layout& desired_layout) {
  for (const auto& sub_mix : candidate_mix->sub_mixes_) {
    for (const auto& layout : sub_mix.layouts) {
      if (layout.loudness_layout == desired_layout) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

// TODO(b/438176780): Ensure this is conformant to IAMF spec §7.3.1.
// TODO(b/438178739): Find a different way of rendering requested layouts not in
// the bitstream.
absl::StatusOr<SelectedMixPresentation> FindMixPresentationAndLayout(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const std::optional<Layout>& desired_layout,
    std::optional<uint32_t> desired_mix_presentation_id) {
  if (supported_mix_presentations.empty()) {
    return absl::InvalidArgumentError("No supported mix presentations found.");
  }

  MixPresentationObu* mix_presentation = nullptr;
  // 1. If given an ID first try to find a matching MixPresentation.
  if (desired_mix_presentation_id.has_value()) {
    for (const auto candidate_mix : supported_mix_presentations) {
      if (*desired_mix_presentation_id ==
          candidate_mix->GetMixPresentationId()) {
        mix_presentation = candidate_mix;
        break;
      }
    }
  }

  // 2. If not given an ID or not found by ID, find by given layout.
  if (mix_presentation == nullptr && desired_layout.has_value()) {
    for (const auto candidate_mix : supported_mix_presentations) {
      if (MixPresentationContainsLayout(candidate_mix, *desired_layout)) {
        mix_presentation = candidate_mix;
        break;
      }
    }
  }

  // 3. By this step if we don't have a MixPresentation, take first.
  if (mix_presentation == nullptr) {
    mix_presentation = supported_mix_presentations.front();
  }
  // Set output then check the selected Mix has at least one submix.
  DecodedUleb128 mix_presentation_id = mix_presentation->GetMixPresentationId();
  if (mix_presentation->sub_mixes_.empty()) {
    return absl::InvalidArgumentError(
        "No submixes found in the selected mix presentation.");
  }

  // 4. Find an output layout either from desired layout or default.
  if (!desired_layout.has_value()) {
    // A. If no desired layout default to the first submix's layout.
    if (mix_presentation->sub_mixes_.front().layouts.empty()) {
      return absl::InvalidArgumentError(
          "No layouts found in the first submix of the first mix "
          "presentation.");
    }
    return SelectedMixPresentation{
        mix_presentation_id,
        mix_presentation->sub_mixes_.front().layouts.front().loudness_layout,
        0,
        0,
    };
  }

  // B. Desired layout specified: Search for it in the selected Mix.
  for (int sub_mix_index = 0;
       sub_mix_index < mix_presentation->sub_mixes_.size(); ++sub_mix_index) {
    const auto& sub_mix = mix_presentation->sub_mixes_[sub_mix_index];
    for (int layout_index = 0; layout_index < sub_mix.layouts.size();
         ++layout_index) {
      const auto& layout = sub_mix.layouts[layout_index];
      if (desired_layout == layout.loudness_layout) {
        return SelectedMixPresentation{
            mix_presentation_id,
            layout.loudness_layout,
            sub_mix_index,
            layout_index,
        };
      }
    }
  }

  // C. Desired layout not found in the Mix so add the first submix.
  mix_presentation->sub_mixes_.front().layouts.push_back(
      {.loudness_layout = *desired_layout});
  return SelectedMixPresentation{

      mix_presentation_id,
      *desired_layout,
      0,
      static_cast<int>(mix_presentation->sub_mixes_.front().layouts.size()) - 1,
  };
}

}  // namespace iamf_tools
