/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/demixing_module.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/audio_element.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/obu_header.h"

namespace iamf_tools {
namespace {

// TODO(b/305927287): Test computation of linear output gains. Test some cases
//                    of erroneous input.

TEST(FindSamplesOrDemixedSamples, FindsMatchingSamples) {
  const std::vector<int32_t> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{"L2", kSamplesToFind}};

  const std::vector<int32_t>* found_samples;
  EXPECT_TRUE(DemixingModule::FindSamplesOrDemixedSamples("L2", kLabelToSamples,
                                                          &found_samples)
                  .ok());
  EXPECT_EQ(*found_samples, kSamplesToFind);
}

TEST(FindSamplesOrDemixedSamples, FindsMatchingDemixedSamples) {
  const std::vector<int32_t> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{"D_L2", kSamplesToFind}};

  const std::vector<int32_t>* found_samples;
  EXPECT_TRUE(DemixingModule::FindSamplesOrDemixedSamples("L2", kLabelToSamples,
                                                          &found_samples)
                  .ok());
  EXPECT_EQ(*found_samples, kSamplesToFind);
}

TEST(FindSamplesOrDemixedSamples, RegularSamplesTakePrecedence) {
  const std::vector<int32_t> kSamplesToFind = {1, 2, 3};
  const std::vector<int32_t> kDemixedSamplesToIgnore = {4, 5, 6};
  const LabelSamplesMap kLabelToSamples = {{"L2", kSamplesToFind},
                                           {"D_L2", kDemixedSamplesToIgnore}};

  const std::vector<int32_t>* found_samples;
  EXPECT_TRUE(DemixingModule::FindSamplesOrDemixedSamples("L2", kLabelToSamples,
                                                          &found_samples)
                  .ok());
  EXPECT_EQ(*found_samples, kSamplesToFind);
}

TEST(FindSamplesOrDemixedSamples, ErrorNoMatchingSamples) {
  const std::vector<int32_t> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{"L2", kSamplesToFind}};

  const std::vector<int32_t>* found_samples;
  EXPECT_EQ(DemixingModule::FindSamplesOrDemixedSamples("L3", kLabelToSamples,
                                                        &found_samples)
                .code(),
            absl::StatusCode::kUnknown);
}

class DemixingModuleTestBase {
 public:
  DemixingModuleTestBase() {
    audio_frame_metadata_.set_audio_element_id(audio_element_id_);
  }

  void TestCreateDemixingModule(int expected_number_of_down_mixers) {
    iamf_tools_cli_proto::UserMetadata user_metadata;
    *user_metadata.add_audio_frame_metadata() = audio_frame_metadata_;
    audio_elements_.emplace(
        audio_element_id_,
        AudioElementWithData{
            .obu = AudioElementObu(ObuHeader(), audio_element_id_,
                                   AudioElementObu::kAudioElementChannelBased,
                                   /*reserved=*/0,
                                   /*codec_config_id=*/0),
            .substream_id_to_labels = substream_id_to_labels_,
        });

    demixing_module_ =
        std::make_unique<DemixingModule>(user_metadata, audio_elements_);

    const std::list<Demixer>* down_mixers = nullptr;
    const std::list<Demixer>* demixers = nullptr;

    ASSERT_TRUE(
        demixing_module_->GetDownMixers(audio_element_id_, down_mixers).ok());
    ASSERT_TRUE(
        demixing_module_->GetDemixers(audio_element_id_, demixers).ok());
    EXPECT_EQ(down_mixers->size(), expected_number_of_down_mixers);
    EXPECT_EQ(demixers->size(), expected_number_of_down_mixers);
  }

 protected:
  void ConfigureAudioFrameMetadata(const std::string& label) {
    // The channel ID itself does not matter. Generate a unique one.
    audio_frame_metadata_.add_channel_ids(
        audio_frame_metadata_.channel_ids_size());
    audio_frame_metadata_.add_channel_labels(label);
  }

  const DecodedUleb128 audio_element_id_ = 137;
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  SubstreamIdLabelsMap substream_id_to_labels_;

  std::unique_ptr<DemixingModule> demixing_module_;
};

class DownMixingModuleTest : public DemixingModuleTestBase,
                             public ::testing::Test {
 protected:
  void TestDownMixing(const DownMixingParams& down_mixing_params,
                      int expected_number_of_down_mixers) {
    TestCreateDemixingModule(expected_number_of_down_mixers);

    EXPECT_TRUE(
        demixing_module_
            ->DownMixSamplesToSubstreams(audio_element_id_, down_mixing_params,
                                         input_label_to_samples_,
                                         substream_id_to_substream_data_)
            .ok());

    for (const auto& [substream_id, substream_data] :
         substream_id_to_substream_data_) {
      // Copy the output queue to a vector for comparison.
      std::vector<std::vector<int32_t>> output_samples;
      std::copy(substream_data.samples_obu.begin(),
                substream_data.samples_obu.end(),
                std::back_inserter(output_samples));
      EXPECT_EQ(output_samples,
                substream_id_to_expected_samples_[substream_id]);
    }
  }

  void ConfigureInputChannel(const std::string& label,
                             const std::vector<int32_t>& input_samples) {
    ConfigureAudioFrameMetadata(label);

    auto [unused_iter, inserted] =
        input_label_to_samples_.emplace(label, input_samples);

    // This function should not be called with the same label twice.
    ASSERT_TRUE(inserted);
  }

  void ConfigureOutputChannel(
      const std::list<std::string>& requested_output_labels,
      const std::vector<std::vector<int32_t>>& expected_output_smples) {
    // The substream ID itself does not matter. Generate a unique one.
    const uint32_t substream_id = substream_id_to_labels_.size();

    substream_id_to_labels_[substream_id] = requested_output_labels;
    substream_id_to_substream_data_[substream_id] = {.substream_id =
                                                         substream_id};

    substream_id_to_expected_samples_[substream_id] = expected_output_smples;
  }

  LabelSamplesMap input_label_to_samples_;

  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data_;

  absl::flat_hash_map<uint32_t, std::vector<std::vector<int32_t>>>
      substream_id_to_expected_samples_;
};

TEST_F(DownMixingModuleTest, OneLayerStereoHasNoDownmixers) {
  ConfigureInputChannel("L2", {});
  ConfigureInputChannel("R2", {});

  ConfigureOutputChannel({"L2", "R2"}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayer7_1_4HasNoDownmixers) {
  // Initialize arguments for single layer 7.1.4.
  ConfigureInputChannel("L7", {});
  ConfigureInputChannel("R7", {});
  ConfigureInputChannel("C", {});
  ConfigureInputChannel("LFE", {});
  ConfigureInputChannel("Lss7", {});
  ConfigureInputChannel("Rss7", {});
  ConfigureInputChannel("Lrs7", {});
  ConfigureInputChannel("Rrs7", {});
  ConfigureInputChannel("Ltf4", {});
  ConfigureInputChannel("Rtf4", {});
  ConfigureInputChannel("Ltb4", {});
  ConfigureInputChannel("Rtb4", {});

  ConfigureOutputChannel({"C"}, {{}});
  ConfigureOutputChannel({"L7", "R7"}, {});
  ConfigureOutputChannel({"Lss7", "Rss7"}, {});
  ConfigureOutputChannel({"Lrs7", "Rrs7"}, {});
  ConfigureOutputChannel({"Ltf4", "Rtf4"}, {});
  ConfigureOutputChannel({"Ltb4", "Rtb4"}, {});
  ConfigureOutputChannel({"LFE"}, {});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, AmbisonicsHasNoDownMixers) {
  ConfigureInputChannel("A0", {});
  ConfigureInputChannel("A1", {});
  ConfigureInputChannel("A2", {});
  ConfigureInputChannel("A3", {});

  ConfigureOutputChannel({"A0"}, {{}});
  ConfigureOutputChannel({"A1"}, {{}});
  ConfigureOutputChannel({"A2"}, {{}});
  ConfigureOutputChannel({"A3"}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayerStereo) {
  ConfigureInputChannel("L2", {0, 1, 2, 3});
  ConfigureInputChannel("R2", {100, 101, 102, 103});

  // Downmix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({"L2", "R2"},
                         {{0, 100}, {1, 101}, {2, 102}, {3, 103}});

  TestDownMixing({}, 0);
}

TEST_F(DownMixingModuleTest, S2ToS1Downmixer) {
  ConfigureInputChannel("L2", {0, 100, 500, 1000});
  ConfigureInputChannel("R2", {100, 0, 500, 500});

  // Downmix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({"L2"}, {{0}, {100}, {500}, {1000}});

  // Downmix to mono as the lowest layer.
  // M = (L2 - 6 dB) + (R2 - 6 dB).
  ConfigureOutputChannel({"M"}, {{50}, {50}, {500}, {750}});

  TestDownMixing({}, 1);
}

TEST_F(DownMixingModuleTest, S3ToS2Downmixer) {
  ConfigureInputChannel("L3", {0, 100});
  ConfigureInputChannel("R3", {0, 100});
  ConfigureInputChannel("C", {100, 100});
  ConfigureInputChannel("Ltf3", {99999, 99999});
  ConfigureInputChannel("Rtf3", {99998, 99998});

  // Downmix to 3.1.2 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({"C"}, {{100}, {100}});
  ConfigureOutputChannel({"Ltf3", "Rtf3"}, {{99999, 99998}, {99999, 99998}});

  // Downmix to stereo as the lowest layer.
  // L2 = L3 + (C - 3 dB).
  // R2 = R3 + (C - 3 dB).
  ConfigureOutputChannel({"L2", "R2"}, {{70, 70}, {170, 170}});

  TestDownMixing({}, 1);
}

TEST_F(DownMixingModuleTest, S5ToS3ToS2Downmixer) {
  ConfigureInputChannel("L5", {100});
  ConfigureInputChannel("R5", {200});
  ConfigureInputChannel("C", {1000});
  ConfigureInputChannel("Ls5", {2000});
  ConfigureInputChannel("Rs5", {3000});
  ConfigureInputChannel("LFE", {6});

  // Downmix to 5.1 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"C"}, {{1000}});
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{2000, 3000}});
  ConfigureOutputChannel({"LFE"}, {{6}});

  // Downmix to stereo as the lowest layer.
  // L3 = L5 + Ls5 * delta.
  // L2 = L3 + (C - 3 dB).
  ConfigureOutputChannel({"L2", "R2"}, {{2221, 3028}});

  // Internally there is a downmixer to L3/R3 then another for L2/R2.
  TestDownMixing({.delta = .707}, 2);
}

TEST_F(DownMixingModuleTest, S5ToS3ToDownmixer) {
  ConfigureInputChannel("L5", {1000});
  ConfigureInputChannel("R5", {2000});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Ls5", {4000});
  ConfigureInputChannel("Rs5", {8000});
  ConfigureInputChannel("Ltf2", {1000});
  ConfigureInputChannel("Rtf2", {2000});
  ConfigureInputChannel("LFE", {8});

  // Downmix to 5.1.2 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{4000, 8000}});

  // Downmix to 3.1.2 as the lowest layer.
  // L3 = L5 + Ls5 * delta.
  ConfigureOutputChannel({"L3", "R3"}, {{3828, 7656}});
  ConfigureOutputChannel({"C"}, {{3}});
  // Ltf3 = Ltf2 + Ls5 * w * delta.
  ConfigureOutputChannel({"Ltf3", "Rtf3"}, {{1707, 3414}});
  ConfigureOutputChannel({"LFE"}, {{8}});

  // Internally there is a downmixer for the height and another for the
  // surround.
  TestDownMixing({.delta = .707, .w = 0.25}, 2);
}

TEST_F(DownMixingModuleTest, T4ToT2Downmixer) {
  ConfigureInputChannel("L5", {1});
  ConfigureInputChannel("R5", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Ls5", {4});
  ConfigureInputChannel("Rs5", {5});
  ConfigureInputChannel("Ltf4", {1000});
  ConfigureInputChannel("Rtf4", {2000});
  ConfigureInputChannel("Ltb4", {1000});
  ConfigureInputChannel("Rtb4", {2000});
  ConfigureInputChannel("LFE", {10});

  // Downmix to 5.1.4 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"Ltb4", "Rtb4"}, {{1000, 2000}});

  // Downmix to 5.1.2 as the lowest layer.
  ConfigureOutputChannel({"L5", "R5"}, {{1, 2}});
  ConfigureOutputChannel({"C"}, {{3}});
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{4, 5}});
  // Ltf2 = Ltf4 + Ltb4 * gamma.
  ConfigureOutputChannel({"Ltf2", "Rtf2"}, {{1707, 3414}});
  ConfigureOutputChannel({"LFE"}, {{10}});

  TestDownMixing({.gamma = .707}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownmixerWithoutT0) {
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("LFE", {8});

  // Downmix to 7.1.0 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"Lrs7", "Rrs7"}, {{3000, 4000}});

  // Downmix to 5.1.0 as the lowest layer.
  ConfigureOutputChannel({"L5", "R5"}, {{1, 2}});
  ConfigureOutputChannel({"C"}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{3598, 5464}});
  ConfigureOutputChannel({"LFE"}, {{8}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownmixerWithT2) {
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf2", {8});
  ConfigureInputChannel("Rtf2", {9});
  ConfigureInputChannel("LFE", {10});

  // Downmix to 7.1.2 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"Lrs7", "Rrs7"}, {{3000, 4000}});

  // Downmix to 5.1.2 as the lowest layer.
  ConfigureOutputChannel({"L5", "R5"}, {{1, 2}});
  ConfigureOutputChannel({"C"}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{3598, 5464}});
  ConfigureOutputChannel({"Ltf2", "Rtf2"}, {{8, 9}});
  ConfigureOutputChannel({"LFE"}, {{10}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownmixerWithT4) {
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf4", {8});
  ConfigureInputChannel("Rtf4", {9});
  ConfigureInputChannel("Ltb4", {10});
  ConfigureInputChannel("Rtb4", {11});
  ConfigureInputChannel("LFE", {12});

  // Downmix to 7.1.4 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({"Lrs7", "Rrs7"}, {{3000, 4000}});

  // Downmix to 5.1.4 as the lowest layer.
  ConfigureOutputChannel({"L5", "R5"}, {{1, 2}});
  ConfigureOutputChannel({"C"}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{3598, 5464}});
  ConfigureOutputChannel({"Ltf4", "Rtf4"}, {{8, 9}});
  ConfigureOutputChannel({"Ltb4", "Rtb4"}, {{10, 11}});
  ConfigureOutputChannel({"LFE"}, {{12}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, SixLayer7_1_4) {
  ConfigureInputChannel("L7", {1000});
  ConfigureInputChannel("R7", {2000});
  ConfigureInputChannel("C", {1000});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf4", {1000});
  ConfigureInputChannel("Rtf4", {2000});
  ConfigureInputChannel("Ltb4", {1000});
  ConfigureInputChannel("Rtb4", {2000});
  ConfigureInputChannel("LFE", {12});

  // There are different paths to have six-layers, choose 7.1.2, 5.1.2, 3.1.2,
  // stereo, mono to avoid dropping the height channels for as many steps as
  // possible.

  // Downmix to 7.1.4 as the sixth layer.
  ConfigureOutputChannel({"Ltb4", "Rtb4"}, {{1000, 2000}});

  // Downmix to 7.1.2 as the fifth layer.
  ConfigureOutputChannel({"Lrs7", "Rrs7"}, {{3000, 4000}});

  // Downmix to 5.1.2 as the fourth layer.
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({"Ls5", "Rs5"}, {{3598, 5464}});

  // Downmix to 3.1.2 as the third layer.
  ConfigureOutputChannel({"C"}, {{1000}});
  // Ltf2 = Ltf4 + Ltb4 * gamma.
  // Ltf3 = Ltf2 + Ls5 * w * delta.
  ConfigureOutputChannel({"Ltf3", "Rtf3"}, {{2644, 4914}});
  ConfigureOutputChannel({"LFE"}, {{12}});

  // Downmix to stereo as the second layer.
  // L5 = L7.
  // L3 = L5 + Ls5 * delta.
  // L2 = L3 + (C - 3 dB).
  ConfigureOutputChannel({"L2"}, {{4822}});

  // Downmix to mono as the first layer.
  // R5 = R7.
  // R3 = R5 + Rs5 * delta.
  // R2 = R3 + (C - 3 dB).
  // M = (L2 - 6 dB) + (R2 - 6 dB).
  ConfigureOutputChannel({"M"}, {{6130}});

  TestDownMixing(
      {.alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25}, 6);
}

class DemixingModuleTest : public DemixingModuleTestBase,
                           public ::testing::Test {
 public:
  void ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      const std::list<std::string>& labels,
      const std::vector<std::vector<int32_t>>& raw_samples,
      DownMixingParams down_mixing_params = {
          .alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25}) {
    // The substream ID itself does not matter. Generate a unique one.
    const uint32_t substream_id = substream_id_to_labels_.size();
    substream_id_to_labels_[substream_id] = labels;

    // Configure a pair of audio frames and decoded audio frames. They share a
    // lot of the same information for a lossless codec.
    audio_frames_.push_back(AudioFrameWithData{
        .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
        .start_timestamp = kStartTimestamp,
        .end_timestamp = kEndTimestamp,
        .raw_samples = raw_samples,
        .down_mixing_params = down_mixing_params,
    });
    decoded_audio_frames_.push_back(
        DecodedAudioFrame{.substream_id = substream_id,
                          .start_timestamp = kStartTimestamp,
                          .end_timestamp = kEndTimestamp,
                          .decoded_samples = raw_samples});

    auto& expected_label_to_samples =
        expected_id_to_time_to_labeled_decoded_frame_[audio_element_id_]
                                                     [kStartTimestamp]
                                                         .label_to_samples;
    // `raw_samples` is arranged in (time, channel axes). Arrange the samples
    // associated with each channel by time. The demixing process never changes
    // data for the input labels.
    auto labels_iter = labels.begin();
    for (int channel = 0; channel < labels.size(); ++channel) {
      auto& samples_for_channel = expected_label_to_samples[*labels_iter];

      samples_for_channel.reserve(raw_samples.size());
      for (auto tick : raw_samples) {
        samples_for_channel.push_back(tick[channel]);
      }
      labels_iter++;
    }
  }

  void ConfiguredExpectedDemixingChannelFrame(
      const std::string& label, std::vector<int32_t> expected_demixed_samples) {
    // Configure the expected demixed channels. Typically the input `label`
    // should have a "D_" prefix.
    expected_id_to_time_to_labeled_decoded_frame_[audio_element_id_]
                                                 [kStartTimestamp]
                                                     .label_to_samples[label] =
        expected_demixed_samples;
  }

  void TestDemixing(int expected_number_of_down_mixers) {
    IdTimeLabeledFrameMap unused_id_to_time_to_labeled_frame,
        id_to_time_to_labeled_decoded_frame;

    TestCreateDemixingModule(expected_number_of_down_mixers);

    EXPECT_TRUE(demixing_module_
                    ->DemixAudioSamples(audio_frames_, decoded_audio_frames_,
                                        unused_id_to_time_to_labeled_frame,
                                        id_to_time_to_labeled_decoded_frame)
                    .ok());

    // Check that the demixed samples have the correct values.
    EXPECT_EQ(id_to_time_to_labeled_decoded_frame[audio_element_id_].size(),
              expected_id_to_time_to_labeled_decoded_frame_[audio_element_id_]
                  .size());
    for (const auto& [time, labeled_frame] :
         id_to_time_to_labeled_decoded_frame[audio_element_id_]) {
      EXPECT_EQ(
          labeled_frame.label_to_samples,
          expected_id_to_time_to_labeled_decoded_frame_[audio_element_id_][time]
              .label_to_samples);
    }
  }

 protected:
  std::list<AudioFrameWithData> audio_frames_;
  std::list<DecodedAudioFrame> decoded_audio_frames_;

  IdTimeLabeledFrameMap expected_id_to_time_to_labeled_decoded_frame_;

 private:
  const int32_t kStartTimestamp = 0;
  const int32_t kEndTimestamp = 1;
};  // namespace

TEST_F(DemixingModuleTest, DemixingAudioSamplesSucceedsWithEmptyInputs) {
  iamf_tools_cli_proto::UserMetadata user_metadata;

  // Clear the inputs.
  audio_elements_.clear();
  demixing_module_ =
      std::make_unique<DemixingModule>(user_metadata, audio_elements_);

  // Call `DemixAudioSamples()`.
  IdTimeLabeledFrameMap id_to_time_to_labeled_frame,
      id_to_time_to_labeled_decoded_frame;
  EXPECT_TRUE(demixing_module_
                  ->DemixAudioSamples(
                      /*audio_frames=*/{},
                      /*decoded_audio_frames=*/{}, id_to_time_to_labeled_frame,
                      id_to_time_to_labeled_decoded_frame)
                  .ok());

  // Expect empty outputs.
  EXPECT_TRUE(id_to_time_to_labeled_frame.empty());
  EXPECT_TRUE(id_to_time_to_labeled_decoded_frame.empty());
}

TEST_F(DemixingModuleTest, AmbisonicsHasNoDemixers) {
  ConfigureAudioFrameMetadata("A0");
  ConfigureAudioFrameMetadata("A1");
  ConfigureAudioFrameMetadata("A2");
  ConfigureAudioFrameMetadata("A3");

  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"A0"}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"A1"}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"A2"}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"A3"}, {{1}});

  TestDemixing(0);
}

TEST_F(DemixingModuleTest, S1ToS2Demixer) {
  // The highest layer is stereo.
  ConfigureAudioFrameMetadata("L2");
  ConfigureAudioFrameMetadata("R2");

  // Mono is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"M"}, {{750}, {1500}});
  // Stereo is the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"L2"}, {{1000}, {2000}});

  // Demixing recovers "D_R2"
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  ConfiguredExpectedDemixingChannelFrame("D_R2", {500, 1000});

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, S2ToS3Demixer) {
  // The highest layer is 3.1.2.
  ConfigureAudioFrameMetadata("L3");
  ConfigureAudioFrameMetadata("R3");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf3");
  ConfigureAudioFrameMetadata("Rtf3");

  // Stereo is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"L2", "R2"},
                                                  {{70, 70}, {1700, 1700}});

  // 3.1.2 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"C"}, {{100}, {1000}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Ltf3", "Rtf3"}, {{99999, 99998}, {99999, 99998}});

  // L3/R3 get demixed from the lower layers.
  // L3 = L2 - (C - 3 dB).
  // R3 = R2 - (C - 3 dB).
  ConfiguredExpectedDemixingChannelFrame("D_L3", {0, 993});
  ConfiguredExpectedDemixingChannelFrame("D_R3", {0, 993});

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, S3ToS5AndTf2ToT2Demixers) {
  // Adding a (valid) layer on top of 3.1.2 will always result in both S3ToS5
  // and Tf2ToT2 demixers.
  // The highest layer is 5.1.2.
  ConfigureAudioFrameMetadata("L5");
  ConfigureAudioFrameMetadata("R5");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf2");
  ConfigureAudioFrameMetadata("Rtf2");

  const DownMixingParams kDownMixingParams = {.delta = .866, .w = 0.25};

  // 3.1.2 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"L3", "R3"}, {{18660, 28660}}, kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"C"}, {{100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Ltf3", "Rtf3"}, {{1000, 2000}}, kDownMixingParams);

  // 5.1.2 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"L5", "R5"}, {{10000, 20000}}, kDownMixingParams);

  // S3ToS5: Ls5/Rs5 get demixed from the lower layers.
  // Ls5 = (1 / delta) * (L3 - L5).
  // Rs5 = (1 / delta) * (R3 - R5).
  ConfiguredExpectedDemixingChannelFrame("D_Ls5", {10000});
  ConfiguredExpectedDemixingChannelFrame("D_Rs5", {10000});

  // Tf2ToT2: Ltf2/Rtf2 get demixed from the lower layers.
  // Ltf2 = Ltf3 - w * (L3 - L5).
  // Rtf2 = Rtf3 - w * (R3 - R5).
  ConfiguredExpectedDemixingChannelFrame("D_Ltf2", {-1165});
  ConfiguredExpectedDemixingChannelFrame("D_Rtf2", {-165});

  TestDemixing(2);
}

TEST_F(DemixingModuleTest, S5ToS7Demixer) {
  // The highest layer is 7.1.0.
  ConfigureAudioFrameMetadata("L7");
  ConfigureAudioFrameMetadata("R7");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Lss7");
  ConfigureAudioFrameMetadata("Rss7");
  ConfigureAudioFrameMetadata("Lrs7");
  ConfigureAudioFrameMetadata("Rrs7");

  const DownMixingParams kDownMixingParams = {.alpha = 0.866, .beta = .866};

  // 5.1.0 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"L5", "R5"}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Ls5", "Rs5"}, {{7794, 7794}}, kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"C"}, {{100}},
                                                  kDownMixingParams);

  // 7.1.0 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Lss7", "Rss7"}, {{1000, 2000}}, kDownMixingParams);

  // L7/R7 get demixed from the lower layers.
  // L7 = R5.
  // R7 = R5.
  ConfiguredExpectedDemixingChannelFrame("D_L7", {100});
  ConfiguredExpectedDemixingChannelFrame("D_R7", {100});

  // Lrs7/Rrs7 get demixed from the lower layers.
  // Lrs7 = (1 / beta) * (Ls5 - alpha * Lss7).
  // Rrs7 = (1 / beta) * (Rs5 - alpha * Rss7).
  ConfiguredExpectedDemixingChannelFrame("D_Lrs7", {8000});
  ConfiguredExpectedDemixingChannelFrame("D_Rrs7", {7000});

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, T2ToT4Demixer) {
  // The highest layer is 5.1.4.
  ConfigureAudioFrameMetadata("L5");
  ConfigureAudioFrameMetadata("R5");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf4");
  ConfigureAudioFrameMetadata("Rtf4");

  const DownMixingParams kDownMixingParams = {.gamma = .866};

  // 5.1.2 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"L5", "R5"}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"Ls5", "Rs5"}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({"C"}, {{100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Ltf2", "Rtf2"}, {{8660, 17320}}, kDownMixingParams);

  // 5.1.4 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {"Ltf4", "Rtf4"}, {{866, 1732}}, kDownMixingParams);

  // Ltb4/Rtb4 get demixed from the lower layers.
  // Ltb4 = (1 / gamma) * (Ltf2 - Ltf4).
  // Ttb4 = (1 / gamma) * (Ttf2 - Rtf4).
  ConfiguredExpectedDemixingChannelFrame("D_Ltb4", {9000});
  ConfiguredExpectedDemixingChannelFrame("D_Rtb4", {18000});

  TestDemixing(1);
}

}  // namespace
}  // namespace iamf_tools
