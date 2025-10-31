#include "iamf/cli/obu_processor_utils.h"

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

// Some re-used convenience constants.
constexpr DecodedUleb128 kMixPresentationId1 = 1;
constexpr DecodedUleb128 kMixPresentationId2 = 2;
constexpr Layout kLayoutA = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
        },
};
constexpr Layout kLayoutB = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system = LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0,
        },
};
constexpr Layout kLayoutC = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system = LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0,
        },
};
constexpr Layout kLayoutD = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system = LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0,
        },
};
constexpr Layout kLayoutE = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system = LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1,
        },
};

// Helper to avoid repetitive boilerplate.
MixPresentationObu CreateMixPresentationObu(
    DecodedUleb128 mix_presentation_id,
    const std::vector<std::vector<Layout>>& submix_layouts) {
  std::vector<MixPresentationSubMix> sub_mixes;
  for (const auto& layouts_for_one_submix : submix_layouts) {
    std::vector<MixPresentationLayout> mix_presentation_layouts;
    for (const auto& layout : layouts_for_one_submix) {
      mix_presentation_layouts.push_back({.loudness_layout = layout});
    }
    sub_mixes.push_back({.audio_elements = {},
                         .output_mix_gain = MixGainParamDefinition(),
                         .layouts = mix_presentation_layouts});
  }

  return MixPresentationObu(ObuHeader(), mix_presentation_id,
                            /*count_label=*/0,
                            /*annotations_language=*/{},
                            /*localized_presentation_annotations=*/{},
                            sub_mixes);
}

// No MixPresentations.
TEST(FindMixPresentationAndLayoutTest, NoMixPresentations) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  EXPECT_THAT(FindMixPresentationAndLayout(supported_mix_presentations,
                                           std::nullopt, std::nullopt),
              Not(IsOk()));
}

// MixPresentation with empty submixes.
TEST(FindMixPresentationAndLayoutTest, EmptySubMixes) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {});
  mix_presentation_1.sub_mixes_.clear();
  supported_mix_presentations.push_back(&mix_presentation_1);

  EXPECT_THAT(FindMixPresentationAndLayout(supported_mix_presentations,
                                           std::nullopt, std::nullopt),
              Not(IsOk()));
}

// The first (default) submix has empty layouts.
TEST(FindMixPresentationAndLayoutTest, EmptyLayouts) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{}, {kLayoutA}});
  supported_mix_presentations.push_back(&mix_presentation_1);

  EXPECT_THAT(FindMixPresentationAndLayout(supported_mix_presentations,
                                           std::nullopt, std::nullopt),
              Not(IsOk()));
}

// ===== Tests with neither ID nor layout specified =====

// Neither ID nor layout specified, should get first, first.
TEST(FindMixPresentationAndLayoutTest, NeitherIdNorLayoutSpecified) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutD}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, std::nullopt, std::nullopt);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
  EXPECT_EQ(result->output_layout, kLayoutA);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 0);
}

// ===== Tests with only layout specified =====

// Only desired_layout is specified and it is found.
TEST(FindMixPresentationAndLayoutTest, LayoutSpecifiedAndFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutD}, {kLayoutE}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, kLayoutE, std::nullopt);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId2);
  EXPECT_EQ(result->output_layout, kLayoutE);
  EXPECT_EQ(result->sub_mix_index, 1);
  EXPECT_EQ(result->layout_index, 0);
}

// Only desired_layout is specified and it is found in multiple mix
// presentations.  It should select the first match.
TEST(FindMixPresentationAndLayoutTest, LayoutSpecifiedAndFoundInMultipleMixes) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA, kLayoutB}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutB}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, kLayoutB, std::nullopt);

  ASSERT_THAT(result, IsOk());
  // Should return the first MixPresentation that has the desired layout.
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
  EXPECT_EQ(result->output_layout, kLayoutB);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 1);
}

// Only desired_layout is specified and not found.
TEST(FindMixPresentationAndLayoutTest, LayoutSpecifiedNotFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutB}, {kLayoutC}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, kLayoutD, std::nullopt);

  ASSERT_THAT(result, IsOk());
  // Should default to the first MixPresentation and add the desired layout.
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 1);
  EXPECT_EQ(result->output_layout, kLayoutD);
  // Verify the layout has been added.
  EXPECT_EQ(mix_presentation_1.sub_mixes_[0].layouts.size(), 2);
  EXPECT_EQ(mix_presentation_1.sub_mixes_[0].layouts[1].loudness_layout,
            kLayoutD);
}

// ===== Tests with only ID specified =====

// Only desired_mix_presentation_id is specified and found.
TEST(FindMixPresentationAndLayoutTest, IdSpecifiedAndFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutB}, {kLayoutC}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, std::nullopt, kMixPresentationId2);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId2);
  // Should default to the first layout of the Mix.
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 0);
  EXPECT_EQ(result->output_layout, kLayoutB);
}

// Only desired_mix_presentation_id is specified but not found.
TEST(FindMixPresentationAndLayoutTest, IdSpecifiedNotFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA}, {kLayoutB}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutC}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  uint32_t desired_mix_presentation = 999;  // Not in the MixPresentations.
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, std::nullopt, desired_mix_presentation);

  ASSERT_THAT(result, IsOk());
  // Should default to the first MixPresentation, first Layout.
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
  EXPECT_EQ(result->output_layout, kLayoutA);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 0);
}

// ===== Tests with both ID and layout specified =====

// Both are specified and found.
TEST(FindMixPresentationAndLayoutTest, BothIdAndLayoutSpecifiedAndFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutA}, {kLayoutB}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, kLayoutB, kMixPresentationId2);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId2);
  EXPECT_EQ(result->output_layout, kLayoutB);
  EXPECT_EQ(result->sub_mix_index, 1);
  EXPECT_EQ(result->layout_index, 0);
}

// Both desired_layout and desired_mix_presentation_id are specified.
// The layout matches the first OBU, but the ID matches the second.
// ID should take precedence and a layout should be added.
TEST(FindMixPresentationAndLayoutTest, IdAndLayoutSpecifiedIdTakesPrecedence) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutB}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutA}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      supported_mix_presentations, kLayoutB, kMixPresentationId2);

  ASSERT_THAT(result, IsOk());
  // Should pick mix_presentation_2 because the ID matches, and use an inserted
  // layout.
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId2);
  EXPECT_EQ(result->output_layout, kLayoutB);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 1);
  // Verify the layout has been added.
  EXPECT_EQ(mix_presentation_2.sub_mixes_.front().layouts.size(), 2);
}

TEST(CreateSimplifiedMixPresentationForRenderingTest,
     SimplifiesToSubMix0Layout1) {
  MixPresentationObu mix_presentation = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});

  absl::StatusOr<MixPresentationObu> result =
      CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                  /*sub_mix_index=*/0,
                                                  /*layout_index=*/1);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->GetMixPresentationId(), kMixPresentationId1);
  ASSERT_EQ(result->sub_mixes_.size(), 1);
  ASSERT_EQ(result->sub_mixes_[0].layouts.size(), 1);
  EXPECT_EQ(result->sub_mixes_[0].layouts[0].loudness_layout, kLayoutB);
}

TEST(CreateSimplifiedMixPresentationForRenderingTest,
     SimplifiesToSubMix1Layout0) {
  MixPresentationObu mix_presentation = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});

  absl::StatusOr<MixPresentationObu> result =
      CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                  /*sub_mix_index=*/1,
                                                  /*layout_index=*/0);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->GetMixPresentationId(), kMixPresentationId1);
  ASSERT_EQ(result->sub_mixes_.size(), 1);
  ASSERT_EQ(result->sub_mixes_[0].layouts.size(), 1);
  EXPECT_EQ(result->sub_mixes_[0].layouts[0].loudness_layout, kLayoutC);
}

TEST(CreateSimplifiedMixPresentationForRenderingTest,
     ReturnsErrorIfSubMixIndexIsOutOfBounds) {
  MixPresentationObu mix_presentation = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});

  EXPECT_THAT(CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                          /*sub_mix_index=*/2,
                                                          /*layout_index=*/0),
              Not(IsOk()));
  EXPECT_THAT(CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                          /*sub_mix_index=*/-1,
                                                          /*layout_index=*/0),
              Not(IsOk()));
}

TEST(CreateSimplifiedMixPresentationForRenderingTest,
     ReturnsErrorIfLayoutIndexIsOutOfBounds) {
  MixPresentationObu mix_presentation = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});

  EXPECT_THAT(CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                          /*sub_mix_index=*/0,
                                                          /*layout_index=*/2),
              Not(IsOk()));
  EXPECT_THAT(CreateSimplifiedMixPresentationForRendering(mix_presentation,
                                                          /*sub_mix_index=*/0,
                                                          /*layout_index=*/-1),
              Not(IsOk()));
}

}  // namespace

}  // namespace iamf_tools
