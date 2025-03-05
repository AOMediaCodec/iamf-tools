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
#ifndef CLI_OBU_SEQUENCER_BASE_H_
#define CLI_OBU_SEQUENCER_BASE_H_

#include <cstdint>
#include <list>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Abstract base class for serializing and writing out OBUs.
 *
 * This class contains functions to serialize and write an IA Sequence. The
 * concrete classes are responsible for packing and writing the output to some
 * output stream.
 *
 * Usage pattern:
 *   // Create a concrete sequencer. Interface is dependent on the conreate
 *   // sequencer.
 *   std::unique_ptr<ObuSequencerBase> sequencer = ...;
 *   // Gather all the OBUs.
 *   // Sequence all of the OBUs.
 *   sequencer->.PickAndPlace(...);
 *
 */
class ObuSequencerBase {
 public:
  /*!\brief Serializes and writes out a temporal unit.
   *
   * Write out the OBUs contained within the input arguments to the output write
   * buffer.
   *
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param temporal_unit Temporal unit to write out.
   * \param wb Write buffer to write to.
   * \param num_samples Number of samples written out.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status WriteTemporalUnit(bool include_temporal_delimiters,
                                        const TemporalUnitView& temporal_unit,
                                        WriteBitBuffer& wb, int& num_samples);

  /*!\brief Writes the input descriptor OBUs.
   *
   * Write out the OBUs contained within the input arguments to the output write
   * buffer.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \param wb Write buffer to write to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status WriteDescriptorObus(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb);

  /*!\brief Constructor.
   *
   * \param leb_generator Leb generator to use when writing OBUs.
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param delay_descriptors_until_first_untrimmed_sample When `true`,
   *        `PushSerializedDescriptorObus` will be delayed until the first
   *        untrimmed sample is pushed.
   */
  ObuSequencerBase(const LebGenerator& leb_generator,
                   bool include_temporal_delimiters,
                   bool delay_descriptors_until_first_untrimmed_sample);

  /*!\brief Destructor.*/
  virtual ~ObuSequencerBase() = 0;

  /*!\brief Pick and places OBUs and write to some output.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param audio_frames Data about Audio Frame OBUs to write.
   * \param parameter_blocks Data about Parameter Block OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PickAndPlace(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const std::list<AudioFrameWithData>& audio_frames,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const std::list<ArbitraryObu>& arbitrary_obus);

 protected:
  /*!\brief Pushes the descriptor OBUs and to some output.
   *
   * Various statistics are also signalled to the concrete class. For example,
   * an MP4 sequencer may need the timing information to control the timebase in
   * the output file. Concrete classes may ignore these statistics as they see
   * fit.
   *
   * \param common_samples_per_frame Common number of samples per frame for the
   *        IA Sequence.
   * \param common_sample_rate Common sample rate for the IA Sequence.
   * \param common_bit_depth Common bit depth for the IA Sequence.
   * \param first_untrimmed_timestamp Timestamp for the first untrimmed sample
   *        in the IA Sequence, or
   *        `delay_descriptors_until_first_untrimmed_sample` is `false`. In some
   *        contexts, this is known as the first Presentation Time Stamp (PTS).
   * \param descriptor_obus Serialized descriptor OBUs to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushSerializedDescriptorObus(
      uint32_t common_samples_per_frame, uint32_t common_sample_rate,
      uint8_t common_bit_depth,
      std::optional<int64_t> first_untrimmed_timestamp, int num_channels,
      absl::Span<const uint8_t> descriptor_obus) = 0;

  /*!\brief Pushes a single temporal unit to some output.
   *
   * \param timestamp Start timestamp of the temporal unit.
   * \param num_samples Number of samples in the temporal unit.
   * \param temporal_unit Temporal unit to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushSerializedTemporalUnit(
      int64_t timestamp, int num_samples,
      absl::Span<const uint8_t> temporal_unit) = 0;

  /*!\brief Signals that no more data is coming. */
  virtual void Flush() = 0;

  /*!\brief Aborts writing the output.
   *
   * Useful for sequencers which want to clean up their output. Such as to avoid
   * leaving a stray file when encoding fails.
   */
  virtual void Abort() = 0;

  // The `LebGenerator` to use when writing OBUs.
  const LebGenerator leb_generator_;

 private:
  enum State { kInitialized, kFlushed };
  State state_ = kInitialized;

  const bool delay_descriptors_until_first_untrimmed_sample_;
  const bool include_temporal_delimiters_;
};

}  // namespace iamf_tools

#endif  // CLI_OBU_SEQUENCER_H_
