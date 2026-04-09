#include "iamf/cli/obu_processor_utils.h"

#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

using enum RenderingConfig::HeadphonesRenderingMode;

// Some re-used convenience constants.
constexpr uint32_t kCodecConfigId = 888;
constexpr uint32_t kAudioElementId = 999;
constexpr uint32_t kAudioElementId2 = 1000;

constexpr std::array<DecodedUleb128, 1> kSubstreamId{100};

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
constexpr Layout kLayout12 = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{
            .sound_system =
                LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0,
        },
};
constexpr Layout kLayoutBinaural = {
    .layout_type = Layout::kLayoutTypeBinaural,
    .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0},
};

// Helper to avoid repetitive boilerplate.
DescriptorObus GetDescriptorObusWithMonoAmbisonics() {
  DescriptorObus descriptor_obus;

  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);

  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, kSubstreamId,
      descriptor_obus.codec_config_obus, descriptor_obus.audio_elements);
  return descriptor_obus;
}

DescriptorObus GetDescriptorObusWithStereoChannelBased(
    uint32_t audio_element_id) {
  DescriptorObus descriptor_obus;

  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);

  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, audio_element_id, kCodecConfigId, kSubstreamId,
      descriptor_obus.codec_config_obus, descriptor_obus.audio_elements);
  return descriptor_obus;
}

DescriptorObus GetDescriptorObusWithBinauralChannelBased(
    uint32_t audio_element_id) {
  DescriptorObus descriptor_obus;

  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);

  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kBinaural, audio_element_id, kCodecConfigId,
      kSubstreamId, descriptor_obus.codec_config_obus,
      descriptor_obus.audio_elements);
  return descriptor_obus;
}

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

MixPresentationObu CreateMixPresentationObuWithAudioElements(
    DecodedUleb128 mix_presentation_id,
    const std::vector<std::vector<Layout>>& submix_layouts,
    const std::vector<SubMixAudioElement>& first_submix_audio_elements) {
  MixPresentationObu obu =
      CreateMixPresentationObu(mix_presentation_id, submix_layouts);
  if (!obu.sub_mixes_.empty()) {
    obu.sub_mixes_[0].audio_elements = first_submix_audio_elements;
  }
  return obu;
}

// No MixPresentations.
TEST(FindMixPresentationAndLayoutTest, NoMixPresentations) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  EXPECT_THAT(FindMixPresentationAndLayout(descriptor_obus.audio_elements,
                                           supported_mix_presentations,
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

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  EXPECT_THAT(FindMixPresentationAndLayout(descriptor_obus.audio_elements,
                                           supported_mix_presentations,
                                           std::nullopt, std::nullopt),
              Not(IsOk()));
}

TEST(FindMixPresentationAndLayoutTest, EmptySubMixesWithExplicitLayout) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {});
  mix_presentation_1.sub_mixes_.clear();
  supported_mix_presentations.push_back(&mix_presentation_1);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  EXPECT_THAT(FindMixPresentationAndLayout(descriptor_obus.audio_elements,
                                           supported_mix_presentations,
                                           kLayoutA, std::nullopt),
              Not(IsOk()));
}

TEST(FindMixPresentationAndLayoutTest, EmptySubMixesWithBinauralLayout) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {});
  mix_presentation_1.sub_mixes_.clear();
  supported_mix_presentations.push_back(&mix_presentation_1);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  EXPECT_THAT(FindMixPresentationAndLayout(descriptor_obus.audio_elements,
                                           supported_mix_presentations,
                                           kLayoutBinaural, std::nullopt),
              Not(IsOk()));
}

// The first (default) submix has empty layouts.
TEST(FindMixPresentationAndLayoutTest, EmptyLayouts) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{}, {kLayoutA}});
  supported_mix_presentations.push_back(&mix_presentation_1);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  EXPECT_THAT(FindMixPresentationAndLayout(descriptor_obus.audio_elements,
                                           supported_mix_presentations,
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

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, std::nullopt,
      std::nullopt);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
  EXPECT_EQ(result->output_layout, kLayoutA);
  EXPECT_EQ(result->sub_mix_index, 0);
  EXPECT_EQ(result->layout_index, 0);
}

// ===== Tests with only layout specified =====

// Tests clause 2.3.1: The specified layout is found.
TEST(FindMixPresentationAndLayoutTest, LayoutSpecifiedAndFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 = CreateMixPresentationObu(
      kMixPresentationId1, {{kLayoutA, kLayoutB}, {kLayoutC}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutD}, {kLayoutE}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, kLayoutE,
      std::nullopt);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->mix_presentation->GetMixPresentationId(),
            kMixPresentationId2);
  EXPECT_EQ(result->output_layout, kLayoutE);
  EXPECT_EQ(result->sub_mix_index, 1);
  EXPECT_EQ(result->layout_index, 0);
}

// Tests clause 2.3.1: Only desired_layout is specified and it is found in
// multiple mix presentations.  It should select the first match.
TEST(FindMixPresentationAndLayoutTest, LayoutSpecifiedAndFoundInMultipleMixes) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA, kLayoutB}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutB}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, kLayoutB,
      std::nullopt);

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

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, kLayoutD,
      std::nullopt);

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

// Tests clause 2.3.1: The specified ID is found.
TEST(FindMixPresentationAndLayoutTest, IdSpecifiedAndFound) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  MixPresentationObu mix_presentation_1 =
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutA}});
  MixPresentationObu mix_presentation_2 =
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutB}, {kLayoutC}});
  supported_mix_presentations.push_back(&mix_presentation_1);
  supported_mix_presentations.push_back(&mix_presentation_2);

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, std::nullopt,
      kMixPresentationId2);

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
  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, std::nullopt,
      desired_mix_presentation);

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

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, kLayoutB,
      kMixPresentationId2);

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

  const DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations, kLayoutB,
      kMixPresentationId2);

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

constexpr uint32_t kStereoAudioElementId = 1234;
const Layout kLayoutStereo = kLayoutA;

SubMixAudioElement CreateSubMixAudioElement(
    uint32_t audio_element_id,
    RenderingConfig::HeadphonesRenderingMode rendering_mode) {
  return {
      .audio_element_id = audio_element_id,
      .localized_element_annotations = {},
      .rendering_config = {.headphones_rendering_mode = rendering_mode,
                           .reserved = 0,
                           .rendering_config_extension_bytes = {}},
      .element_mix_gain = MixGainParamDefinition(),
  };
}

SelectedMixPresentation FindStereoMixPresentationExpectOk(
    DescriptorObus& descriptor_obus) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  for (auto& mix : descriptor_obus.mix_presentation_obus) {
    supported_mix_presentations.push_back(&mix);
  }

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations,
      kLayoutStereo, std::nullopt);

  EXPECT_THAT(result, IsOk());
  return *result;
}

SelectedMixPresentation FindBinauralMixPresentationExpectOk(
    DescriptorObus& descriptor_obus) {
  std::list<MixPresentationObu*> supported_mix_presentations;
  for (auto& mix : descriptor_obus.mix_presentation_obus) {
    supported_mix_presentations.push_back(&mix);
  }

  absl::StatusOr<SelectedMixPresentation> result = FindMixPresentationAndLayout(
      descriptor_obus.audio_elements, supported_mix_presentations,
      kLayoutBinaural, std::nullopt);

  EXPECT_THAT(result, IsOk());
  return *result;
}

TEST(FindMixPresentationAndLayoutTest,
     SelectsFirstMixPresentationWithoutArtistPreferredStereoMix) {
  DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  constexpr uint32_t kFirstMixPresentationId = 9999;
  descriptor_obus.mix_presentation_obus = {
      CreateMixPresentationObu(kFirstMixPresentationId, {{kLayoutE}}),
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutD}})};

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kFirstMixPresentationId);
}

// Tests clause 2.2.1: Find exact stereo match (stereo layout + stereo AE +
// identical headphones rendering mode).
TEST(FindMixPresentationAndLayoutTest,
     SelectsArtistPreferredStereoMixWhenAvailable) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  constexpr uint32_t kPreferredStereoMixPresentationId = 9999;
  // Non-stereo mix.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutE}}));
  // Preferred stereo mix.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kPreferredStereoMixPresentationId, {{kLayoutA}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredStereoMixPresentationId);
}

// Tests clause 2.2.1: Prefers mix with exactly one stereo layout.
TEST(FindMixPresentationAndLayoutTest,
     SelectsArtistPreferredMixWithOneStereoLayout) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  constexpr uint32_t kPreferredStereoMixPresentationId = 9999;
  // Mix with two layouts.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kMixPresentationId1, {{kLayoutStereo, kLayout12}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));
  // Preferred mix with one stereo layout.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kPreferredStereoMixPresentationId, {{kLayoutA}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredStereoMixPresentationId);
}

// Tests clause 2.2.1: Prefers mix with exactly one stereo audio element.
TEST(FindMixPresentationAndLayoutTest, SelectsArtistPrefersOneAudioElementMix) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId2, kCodecConfigId, kSubstreamId,
      descriptor_obus.codec_config_obus, descriptor_obus.audio_elements);
  constexpr uint32_t kPreferredStereoMixPresentationId = 9999;
  // Mix stereo and ambisonics audio elements using a stereo layout.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kMixPresentationId1, {{kLayoutE}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo),
           CreateSubMixAudioElement(kAudioElementId2,
                                    kHeadphonesRenderingModeStereo)}));
  // Preferred stereo mix presentation.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kPreferredStereoMixPresentationId, {{kLayoutA}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredStereoMixPresentationId);
}

// Tests clause 2.2.1: Prefers mix with correct headphones rendering mode.
TEST(FindMixPresentationAndLayoutTest,
     ArtistPreferredStereoPrefersStereoRenderingModeOverBinaural) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  constexpr uint32_t kPreferredRenderingModeMixPresentationId = 9999;
  // Mix with binaural headphones rendering mode.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kMixPresentationId1, {{kLayoutStereo}},
          {CreateSubMixAudioElement(
              kStereoAudioElementId,
              kHeadphonesRenderingModeBinauralWorldLocked)}));
  // Preferred mix with stereo headphones rendering mode.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kPreferredRenderingModeMixPresentationId, {{kLayoutStereo}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredRenderingModeMixPresentationId);
}

// Tests clause 2.2.2: Find relaxed stereo match (stereo layout + stereo AE +
// ignores headphones mode mismatch).
TEST(FindMixPresentationAndLayoutTest,
     ArtistPreferredStereoFallsBackWhenStereoRenderingModeNotAvailable) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  constexpr uint32_t kFallbackMixPresentationId = 9999;
  // Non-stereo mix.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutE}}));
  // "Back-up" mix with a binaural rendering mode. Intent would be better
  // signalled by using stereo headphones rendering mode.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kFallbackMixPresentationId, {{kLayoutStereo}},
          {CreateSubMixAudioElement(
              kStereoAudioElementId,
              kHeadphonesRenderingModeBinauralWorldLocked)}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kFallbackMixPresentationId);
}

TEST(FindMixPresentationAndLayoutTest, GracefullyBypassesMissingAudioElement) {
  constexpr uint32_t kMissingAudioElementId = 1235;
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kMissingAudioElementId);

  // Gracefully handle this mix, which has no matching audio element.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kMixPresentationId1, {{kLayoutStereo}},
          {CreateSubMixAudioElement(kMissingAudioElementId,
                                    kHeadphonesRenderingModeStereo)}));
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutStereo}}));

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kMixPresentationId1);
}

TEST(FindMixPresentationAndLayoutTest,
     ArtistPreferredStereoIsBasedOnFirstSubMix) {
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithStereoChannelBased(kStereoAudioElementId);
  // Non-stereo mix.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutE}}));
  // Mimics the "artist input audio".
  auto mix_presentation_with_two_sub_mixes =
      CreateMixPresentationObuWithAudioElements(
          kStereoAudioElementId, {{kLayoutStereo}},
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)});
  // Simulate system sound being added after the artist input audio.
  mix_presentation_with_two_sub_mixes.sub_mixes_.push_back(
      MixPresentationSubMix{
          {CreateSubMixAudioElement(kStereoAudioElementId,
                                    kHeadphonesRenderingModeStereo)},
          MixGainParamDefinition(),
          {{kLayoutStereo}}});
  descriptor_obus.mix_presentation_obus.push_back(
      mix_presentation_with_two_sub_mixes);

  const SelectedMixPresentation result =
      FindStereoMixPresentationExpectOk(descriptor_obus);

  // The spec does not say how to handle multiple sub-mixes, we choose to ignore
  // the second sub-mix, to work better with the "system sound" use case.
  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kStereoAudioElementId);
}

// Tests clause 2.1.1: Select the mix with exactly one binaural audio element.
TEST(FindMixPresentationAndLayoutTest,
     SelectsBinauralMixWithExactlyOneBinauralAudioElement) {
  constexpr uint32_t kBinauralAudioElementId = 2222;
  DescriptorObus descriptor_obus =
      GetDescriptorObusWithBinauralChannelBased(kBinauralAudioElementId);
  constexpr uint32_t kPreferredBinauralMixPresentationId = 9999;
  // Stereo layout.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutStereo}}));
  // Binaural layout, but no audio elements.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId2, {{kLayoutBinaural}}));
  // Preferred binaural mix (exactly one binaural audio element).
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObuWithAudioElements(
          kPreferredBinauralMixPresentationId, {{kLayoutStereo}},
          {CreateSubMixAudioElement(
              kBinauralAudioElementId,
              kHeadphonesRenderingModeBinauralWorldLocked)}));

  const SelectedMixPresentation result =
      FindBinauralMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredBinauralMixPresentationId);
}

// Tests clause 2.1.2: Select the mix with the binaural loudness layout.
TEST(FindMixPresentationAndLayoutTest,
     SelectsBinauralMixByLoudnessLayoutFallback) {
  DescriptorObus descriptor_obus = GetDescriptorObusWithMonoAmbisonics();
  constexpr uint32_t kPreferredBinauralMixPresentationId = 8888;
  // Stereo layout.
  descriptor_obus.mix_presentation_obus.push_back(
      CreateMixPresentationObu(kMixPresentationId1, {{kLayoutStereo}}));
  // Preferred mix with a binaural layout.
  descriptor_obus.mix_presentation_obus.push_back(CreateMixPresentationObu(
      kPreferredBinauralMixPresentationId, {{kLayoutBinaural}}));

  const SelectedMixPresentation result =
      FindBinauralMixPresentationExpectOk(descriptor_obus);

  EXPECT_EQ(result.mix_presentation->GetMixPresentationId(),
            kPreferredBinauralMixPresentationId);
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
