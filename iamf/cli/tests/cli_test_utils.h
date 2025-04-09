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
#ifndef CLI_TESTS_CLI_TEST_UTILS_H_
#define CLI_TESTS_CLI_TEST_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Processes the input standalone IAMF Sequence to output containers.
 *
 * This function is useful for testing whether a generated IAMF Sequence
 * contains expected OBUs.
 *
 * \param read_bit_buffer Buffer reader that reads the IAMF bitstream. The
 *        reader's position will be moved past the first IA sequence.
 * \param sequence_header Output IA sequence header.
 * \param codec_config_obus Output codec configs.
 * \param audio_elements Output audio elements.
 * \param mix_presentations Output mix presentations.
 * \param audio_frames Output audio frames.
 * \param parameter_blocks Output parameter blocks.
 * \return `absl::OkStatus()` if the process is successful. A specific status
 *         on failure.
 */
absl::Status CollectObusFromIaSequence(
    ReadBitBuffer& read_bit_buffer, IASequenceHeaderObu& ia_sequence_header,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentations,
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks);

// A specification for a decode request. Currently used in the context of
// extracting the relevant metadata from the UserMetadata proto associated
// with a given test vector.
struct DecodeSpecification {
  uint32_t mix_presentation_id;
  uint32_t sub_mix_index;
  LoudspeakersSsConventionLayout::SoundSystem sound_system;
  uint32_t layout_index;
};

/*!\brief Adds a configurable LPCM `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param num_samples_per_frame Number of samples per frame.
 * \param sample_size Sample size.
 * \param sample_rate `sample_rate` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddLpcmCodecConfig(
    DecodedUleb128 codec_config_id, uint32_t num_samples_per_frame,
    uint8_t sample_size, uint32_t sample_rate,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable LPCM `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param sample_rate `sample_rate` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddLpcmCodecConfigWithIdAndSampleRate(
    uint32_t codec_config_id, uint32_t sample_rate,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable Opus `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddOpusCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable Flac `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddFlacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable AAC `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddAacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable ambisonics `AudioElementObu` to the output.
 *
 * \param audio_element_id `audio_element_id` of the OBU to create.
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param substream_ids `substream_ids` of the OBU to create.
 * \param codec_config_obus Codec Config OBUs containing the associated OBU.
 * \param audio_elements Map to add the OBU to keyed by `audio_element_id`.
 */
void AddAmbisonicsMonoAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    absl::Span<const DecodedUleb128> substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements);

/*!\brief Adds a configurable scalable `AudioElementObu` to the output argument.
 *
 * \param input_layout `input_layout` of the OBU to create.
 * \param audio_element_id `audio_element_id` of the OBU to create.
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param substream_ids `substream_ids` of the OBU to create.
 * \param codec_config_obus Codec Config OBUs containing the associated OBU.
 * \param audio_elements Map to add the OBU to keyed by `audio_element_id`.
 */
void AddScalableAudioElementWithSubstreamIds(
    IamfInputLayout input_layout, DecodedUleb128 audio_element_id,
    uint32_t codec_config_id, absl::Span<const DecodedUleb128> substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements);

/*!\brief Adds a configurable `MixPresentationObu` to the output argument.
 *
 * \param mix_presentation_id `mix_presentation_id` of the OBU to create.
 * \param audio_element_ids `audio_element_id`s of the OBU to create.
 * \param common_parameter_id `parameter_id` of all parameters within the
 *        created OBU.
 * \param common_parameter_rate `parameter_rate` of all parameters within the
 *        created OBU.
 * \param output_mix_presentations List to add OBU to.
 */
void AddMixPresentationObuWithAudioElementIds(
    DecodedUleb128 mix_presentation_id,
    const std::vector<DecodedUleb128>& audio_element_id,
    DecodedUleb128 common_parameter_id, DecodedUleb128 common_parameter_rate,
    std::list<MixPresentationObu>& output_mix_presentations);

/*!\brief Adds a configurable `MixPresentationObu` to the output argument.
 *
 * \param mix_presentation_id `mix_presentation_id` of the OBU to create.
 * \param audio_element_ids `audio_element_id`s of the OBU to create.
 * \param common_parameter_id `parameter_id` of all parameters within the
 *        created OBU.
 * \param common_parameter_rate `parameter_rate` of all parameters within the
 *        created OBU.
 * \param sound_system_layouts `sound_system`s of the OBU to create.
 * \param output_mix_presentations List to add OBU to.
 */
void AddMixPresentationObuWithConfigurableLayouts(
    DecodedUleb128 mix_presentation_id,
    const std::vector<DecodedUleb128>& audio_element_id,
    DecodedUleb128 common_parameter_id, DecodedUleb128 common_parameter_rate,
    const std::vector<LoudspeakersSsConventionLayout::SoundSystem>&
        sound_system_layouts,
    std::list<MixPresentationObu>& output_mix_presentations);

/*!\brief Adds a configurable mix gain param definition to the output argument.
 *
 * \param parameter_id `parameter_id` of the param definition to create.
 * \param parameter_rate `parameter_rate` of the param definition to
 *        create.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        param definition to create.
 * \param param_definitions Map to add the param definition to keyed by
 *        `parameter_id`.
 */
void AddParamDefinitionWithMode0AndOneSubblock(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration,
    absl::flat_hash_map<DecodedUleb128, MixGainParamDefinition>&
        param_definitions);

/*!\brief Adds a demixing parameter definition to an Audio Element OBU.
 *
 * \param parameter_id `parameter_id` of the param definition to add.
 * \param parameter_rate `parameter_rate` of the param definition to add.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        param definition to add.
 * \param audio_element_obu Audio Element OBU to add the param definition to.
 */
void AddDemixingParamDefinition(DecodedUleb128 parameter_id,
                                DecodedUleb128 parameter_rate,
                                DecodedUleb128 duration,
                                AudioElementObu& audio_element_obu);

/*!\brief Adds a recon gain parameter definition to an Audio Element OBU.
 *
 * \param parameter_id `parameter_id` of the param definition to add.
 * \param parameter_rate `parameter_rate` of the param definition to add.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        param definition to add.
 * \param audio_element_obu Audio Element OBU to add the param definition to.
 */
void AddReconGainParamDefinition(DecodedUleb128 parameter_id,
                                 DecodedUleb128 parameter_rate,
                                 DecodedUleb128 duration,
                                 AudioElementObu& audio_element_obu);

/*!\brief Calls `CreateWavReader` and unwraps the `StatusOr`.
 *
 * \param filename Filename to forward to `CreateWavReader`.
 * \param num_samples_per_frame Number of samples per frame to forward to
 *        `CreateWavReader`.
 * \return Unwrapped `WavReader` created by `CreateWavReader`.
 */
WavReader CreateWavReaderExpectOk(const std::string& filename,
                                  int num_samples_per_frame = 1);

/*!\brief Renders the `LabeledFrame` flushes to the output vector.
 *
 * \param labeled_frame Labeled frame to render.
 * \param renderer Renderer to use.
 * \param output_samples Vector to flush to.
 */
void RenderAndFlushExpectOk(const LabeledFrame& labeled_frame,
                            AudioElementRendererBase* renderer,
                            std::vector<InternalSampleType>& output_samples);

/*!\brief Gets and cleans up unique file name based on the specified suffix.
 *
 * Useful when testing components that write to a single file.
 *
 * \param suffix Suffix to append to the file path.
 * \return Unique file path based on the current unit test info.
 */
std::string GetAndCleanupOutputFileName(absl::string_view suffix);

/*!\brief Gets and creates a unique directory based on the specified suffix.
 *
 * Useful when testing components that write several files to a single
 * directory.
 *
 * \param suffix Suffix to append to the directory.
 * \return Unique file path based on the current unit test info.
 */
std::string GetAndCreateOutputDirectory(absl::string_view suffix);

/*!\brief Serializes a list of OBUs.
 *
 * \param obus OBUs to serialize.
 * \param leb_generator Leb generator to use.
 * \return Vector of serialized OBU data.
 */
std::vector<uint8_t> SerializeObusExpectOk(
    const std::list<const ObuBase*>& obus,
    const LebGenerator& leb_generator = *LebGenerator::Create());

/*!\brief Parses a textproto file into a `UserMetadata` proto.
 *
 * This function also asserts that the file exists and is readable.
 *
 * \param textproto_filename File to parse.
 * \param user_metadata Proto to populate.
 */
void ParseUserMetadataAssertSuccess(
    const std::string& textproto_filename,
    iamf_tools_cli_proto::UserMetadata& user_metadata);

/*!\brief Computes the log-spectral distance (LSD) between two spectra.
 *
 * The log-spectral distance (LSD) is a distance measure (expressed in dB)
 * between two spectra.
 *
 * \param first_log_spectrum First log-spectrum to compare.
 * \param second_log_spectrum Second log-spectrum to compare.
 * \return Log-spectral distance between the two spectra.
 */
double GetLogSpectralDistance(
    const absl::Span<const InternalSampleType>& first_log_spectrum,
    const absl::Span<const InternalSampleType>& second_log_spectrum);

/*!\brief Extracts the relevant metadata for a given test case.
 *
 * This is used to properly associate gold-standard wav files with the output of
 * the decoder.
 *
 * \param user_metadata Proto associated with a given test vector.
 * \return `DecodeSpecification`(s) for the given test case.
 */
std::vector<DecodeSpecification> GetDecodeSpecifications(
    const iamf_tools_cli_proto::UserMetadata& user_metadata);

/*!\brief Converts a span of `int32_t` to a span of `InternalSampleType`.
 *
 * Useful because some test data is more readable as `int32_t`s, than in the
 * canonical `InternalSampleType` format.
 *
 * \param samples Span of `int32_t`s to convert.
 * \param result Span of `InternalSampleType`s to write to.
 */
constexpr void Int32ToInternalSampleType(
    absl::Span<const int32_t> samples, absl::Span<InternalSampleType> result) {
  std::transform(samples.begin(), samples.end(), result.begin(),
                 Int32ToNormalizedFloatingPoint<InternalSampleType>);
}

/*!\brief Converts a span of `int32_t` to a span of `InternalSampleType`.
 *
 * Useful because some test data is more readable as `int32_t`s, than in the
 * canonical `InternalSampleType` format.
 *
 * \param samples Span of `int32_t`s to convert.
 * \return Output vector of `InternalSampleType`s.
 */
std::vector<InternalSampleType> Int32ToInternalSampleType(
    absl::Span<const int32_t> samples);

/*!\brief Returns samples representing a sine wave.
 *
 * \param start_tick Tick to start sampling at. I.e. each tick represents
 *                   `1.0 / sample_rate_hz` seconds.
 * \param num_samples Number of samples to generate.
 * \param sample_rate_hz Sample rate of the generated samples in Hz.
 * \param frequency_hz Frequency of the sine wave in Hz.
 * \param amplitude Amplitude of the sine wave. Recommended to be in [-1.0,
 *                  1.0] to agree with the canonical `InternalSampleType`
 *                  convention.
 * \return Output vector of `InternalSampleType`s.
 */
std::vector<InternalSampleType> GenerateSineWav(uint64_t start_tick,
                                                uint32_t num_samples,
                                                uint32_t sample_rate_hz,
                                                double frequency_hz,
                                                double amplitude);

/*!\brief Counts the zero crossings for each channel.
 *
 * The first time a user calls this, the `zero_crossing_states` and
 * `zero_crossing_counts` may be empty. In subsequent calls, the user should
 * pass the previous state of each channel.
 *
 * This pattern allows the user to accumulate the zero crossings for a
 * single audio channel, while allowing data to be processed in chunks (i.e.
 * frames).
 *
 * \param tick_channel_samples Samples arranged in (time, channel) axes.
 * \param zero_crossing_states Initial state for each channel. Used between
 *        subsequence calls to `CountZeroCrossings` to track the state of each
 *        channel.
 * \param zero_crossing_counts Accumulates the number of zero crossings
 *        detected.
 */
enum class ZeroCrossingState { kUnknown, kPositive, kNegative };
void AccumulateZeroCrossings(
    absl::Span<const std::vector<int32_t>> tick_channel_samples,
    std::vector<ZeroCrossingState>& zero_crossing_states,
    std::vector<int>& zero_crossing_counts);
/*!\brief Reads the contents of the file and appends it to `buffer`.
 *
 * \param file_path Path of file to read.
 * \param buffer Buffer to append the contents of the file to.
 * \return `absl::OkStatus()` on success. A specific error code on failure.
 */
absl::Status ReadFileToBytes(const std::filesystem::path& file_path,
                             std::vector<uint8_t>& buffer);

/*!\brief Matches an `InternalSampleType` to an `int32_t`..
 *
 * Used with a tuple of `InternalSampleType` and `int32_t`.
 *
 * For example:
 *    std::vector<InternalSampleType> samples;
 *    std::vector<int32_t> expected_samples;
 *    EXPECT_THAT(samples,
 *                Pointwise(InternalSampleMatchesIntegralSample(),
 *                          expected_samples));
 */
MATCHER(InternalSampleMatchesIntegralSample, "") {
  int32_t equivalent_integral_sample;
  return NormalizedFloatingPointToInt32(testing::get<0>(arg),
                                        equivalent_integral_sample)
             .ok() &&
         equivalent_integral_sample == testing::get<1>(arg);
}

/*!\brief Matches a tag that is the build information of the IAMF encoder.
 *
 * A matcher that checks that the tag name is "iamf_encoder" and the tag value
 * starts with the prefix of the build information of the IAMF encoder. In the
 * future we may add a suffix, such as the commit hash, to the tag value. This
 * matcher will match both the old and new formats.
 *
 * For example:
 * const MixPresentationTags::Tag tag{.tag_name = "iamf_encoder",
 *                                    .tag_value = "GitHub/iamf-tools"};
 * EXPECT_THAT(tag, TagMatchesBuildInformation());
 */
MATCHER(TagMatchesBuildInformation, "") {
  constexpr absl::string_view kIamfEncoderBuildInformationPrefix =
      "GitHub/iamf-tools";
  return arg.tag_name == "iamf_encoder" &&
         ExplainMatchResult(
             ::testing::StartsWith(kIamfEncoderBuildInformationPrefix),
             arg.tag_value, result_listener);
}

/*!\brief A mock sample processor. */
class MockSampleProcessor : public SampleProcessorBase {
 public:
  MockSampleProcessor(uint32_t max_input_samples_per_frame, size_t num_channels,
                      uint32_t max_output_samples_per_frame)
      : SampleProcessorBase(max_input_samples_per_frame, num_channels,
                            max_output_samples_per_frame) {}

  MOCK_METHOD(absl::Status, PushFrameDerived,
              (absl::Span<const std::vector<int32_t>> time_channel_samples),
              (override));

  MOCK_METHOD(absl::Status, FlushDerived, (), (override));
};

/*!\brief A simple processor which resamples the output to every second tick.
 */
class EverySecondTickResampler : public SampleProcessorBase {
 public:
  EverySecondTickResampler(uint32_t max_input_num_samples_per_frame,
                           size_t num_channels)
      : SampleProcessorBase(max_input_num_samples_per_frame, num_channels,
                            /*max_output_samples_per_frame=*/
                            max_input_num_samples_per_frame / 2) {}

 private:
  /*!\brief Pushes a frame of samples to be resampled.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushFrameDerived(
      absl::Span<const std::vector<int32_t>> time_channel_samples) override;

  /*!\brief Signals to close the resampler and flush any remaining samples.
   *
   * It is bad practice to reuse the resampler after calling this function.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status FlushDerived() override;
};

/*!\brief A simple processor which delays the output by one frame.
 *
 * Useful for tests which want to verify that an abstract `SampleProcessorBase`
 * is properly being used when it has delayed output.
 *
 * In real-world use cases, resamplers and loudness limiters often will have a
 * short delay in their output, which `SampleProcessorBase` permits. This is
 * just simple implementation of a delayer which helps ensure that any delayed
 * samples are not lost.
 */
class OneFrameDelayer : public SampleProcessorBase {
 public:
  /*!\brief Constructor.
   *
   * \param max_input_samples_per_frame Maximum number of samples per frame in
   *        the input timescale. Later calls to `PushFrame()` must contain at
   *        most this many samples.
   * \param num_channels Number of channels. Later calls to `PushFrame()` must
   *        contain this many channels.
   */
  OneFrameDelayer(uint32_t max_input_num_samples_per_frame, size_t num_channels)
      : SampleProcessorBase(max_input_num_samples_per_frame, num_channels,
                            /*max_output_samples_per_frame=*/
                            max_input_num_samples_per_frame),
        delayed_samples_(max_input_num_samples_per_frame,
                         std::vector<int32_t>(num_channels)) {}

 private:
  /*!\brief Pushes a frame of samples to be resampled.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushFrameDerived(
      absl::Span<const std::vector<int32_t>> time_channel_samples) override;

  /*!\brief Signals to close the resampler and flush any remaining samples.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status FlushDerived() override;

  // Buffer to track the delayed samples.
  std::vector<std::vector<int32_t>> delayed_samples_;
  size_t num_delayed_ticks_ = 0;
};

/*!\brief A mock loudness calculator factory. */
class MockLoudnessCalculatorFactory : public LoudnessCalculatorFactoryBase {
 public:
  MockLoudnessCalculatorFactory() : LoudnessCalculatorFactoryBase() {}

  MOCK_METHOD(std::unique_ptr<LoudnessCalculatorBase>, CreateLoudnessCalculator,
              (const MixPresentationLayout& layout,
               uint32_t num_samples_per_frame, int32_t rendered_sample_rate,
               int32_t rendered_bit_depth),
              (const, override));
};

/*!\brief A mock loudness calculator. */
class MockLoudnessCalculator : public LoudnessCalculatorBase {
 public:
  MockLoudnessCalculator() : LoudnessCalculatorBase() {}

  MOCK_METHOD(absl::Status, AccumulateLoudnessForSamples,
              (absl::Span<const std::vector<int32_t>> time_channel_samples),
              (override));

  MOCK_METHOD(absl::StatusOr<LoudnessInfo>, QueryLoudness, (),
              (const, override));
};

/*!\brief A mock sample processor factory. */
typedef testing::MockFunction<std::unique_ptr<SampleProcessorBase>(
    DecodedUleb128 mix_presentation_id, int sub_mix_index, int layout_index,
    const Layout& layout, int num_channels, int sample_rate, int bit_depth,
    size_t num_samples_per_frame)>
    MockSampleProcessorFactory;

/*!\brief A mock OBU sequencer. */
class MockObuSequencer : public ObuSequencerBase {
 public:
  /*!\brief Constructor.
   *
   * \param leb_generator Leb generator to use when writing OBUs.
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param delay_descriptors_until_first_untrimmed_sample Whether the
   *        descriptor OBUs should be delayed until the first untrimmed frame
   *        is known.
   */
  MockObuSequencer(const LebGenerator& leb_generator,
                   bool include_temporal_delimiters,
                   bool delay_descriptors_until_first_untrimmed_sample)
      : ObuSequencerBase(leb_generator, include_temporal_delimiters,
                         delay_descriptors_until_first_untrimmed_sample) {}

  MOCK_METHOD(void, AbortDerived, (), (override));

  MOCK_METHOD(absl::Status, PushSerializedDescriptorObus,
              (uint32_t common_samples_per_frame, uint32_t common_sample_rate,
               uint8_t common_bit_depth,
               std::optional<InternalTimestamp> first_untrimmed_timestamp,
               int num_channels, absl::Span<const uint8_t> descriptor_obus),
              (override));

  MOCK_METHOD(absl::Status, PushSerializedTemporalUnit,
              (InternalTimestamp timestamp, int num_samples,
               absl::Span<const uint8_t> temporal_unit),
              (override));

  MOCK_METHOD(absl::Status, PushFinalizedDescriptorObus,
              (absl::Span<const uint8_t> descriptor_obus), (override));

  MOCK_METHOD(void, CloseDerived, (), (override));
};

}  // namespace iamf_tools

#endif  // CLI_TESTS_CLI_TEST_UTILS_H_
