/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef API_IAMF_DECODER_H_
#define API_IAMF_DECODER_H_

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
namespace iamf_tools {

class IamfDecoder {
 public:
  /*!\brief Determines the format of the output file. */
  enum OutputFileBitDepth {
    kBitDepthAutomatic,  // Automatically determine based on the bit-depth of
                         // the input file.
    kBitDepth16,
    kBitDepth24,
    kBitDepth32,
  };

  // TODO(b/339500539): Add support for other IAMF supported layouts
  /*!\brief Determines the layout of the output file.
   *
   * Typically these correspond with `sound_system`s in the IAMF spec
   * (https://aomediacodec.github.io/iamf/#syntax-layout).
   */
  enum OutputLayout {
    kOutputStereo,
  };

  /*!\brief Metadata that describes a mix presentation.
   *
   * Used by a user to determine which mix presentation they would like to
   * configure the decoder with.
   */
  struct MixPresentationMetadata {
    uint32_t mix_presentation_id;
    MixPresentationTags mix_presentation_tags;
  };

  /* WARNING: API is currently in flux and will change.
   *
   * The functions below constitute our IAMF Iterative Decoder API. Below is a
   * sample usage of the API.
   *
   * Reconfigurable Standalone IAMF Usage
   * IamfDecoder streaming_decoder = IamfDecoder::Create();
   * for chunk of data in iamf stream:
   *    Decode()
   *    if (IsDescriptorProcessingComplete()) {
   *      GetMixPresentations(output_mix_presentation_ids)
   *      ConfigureMixPresentationId(mix_presentation_id)
   *      ConfigureOutputLayout(output_layout)
   *      ConfigureBitDepth(bit_depth)
   *    }
   * for chunk of data in iamf stream:
   *    Decode()
   *    while (IsTemporalUnitAvailable()) {
   *      GetOutputTemporalUnit(output_temporal_unit)
   *      Playback(output_temporal_unit)
   *    }
   * while (IsTemporalUnitAvailable()) {
   *      Flush(output_temporal_unit)
   *      Playback(output_temporal_unit)
   *  }
   * Close();
   */

  /*!\brief Creates an IamfDecoder.
   *
   * This function should be used for pure streaming applications in which the
   * descriptor OBUs are not known in advance.
   *
   * \return IamfDecoder upon success. Other specific statuses on
   *         failure.
   */
  static absl::StatusOr<IamfDecoder> Create();

  /*!\brief Creates an IamfDecoder from a known set of descriptor OBUs.
   *
   * This function should be used for applications in which the descriptor OBUs
   * are known in advance.
   *
   * \param descriptor_obus Bitstream containing all the descriptor OBUs and
   *        only descriptor OBUs.
   * \return IamfDecoder upon success. Other specific statuses on
   *         failure.
   */
  static absl::StatusOr<IamfDecoder> CreateFromDescriptors(
      absl::Span<const uint8_t> descriptor_obus);

  /*!\brief Configures the decoder with the desired mix presentation.
   *
   * \param mix_presentation_id Specifies the desired mix presentation.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status ConfigureMixPresentationId(DecodedUleb128 mix_presentation_id);

  /*!\brief Configures the decoder with the desired output layout.
   *
   * \param output_layout Specifies the desired output layout.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status ConfigureOutputLayout(OutputLayout output_layout);

  /*!\brief Configures the decoder with the desired bit depth.
   *
   * \param bit_depth Specifies the desired bit depth.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  // TODO(b/379124235): Update OutputFileBitDepth to OutputBitDepth to
  //                  indicate that this is not specific to file-based decoding.
  // TODO(b/379122580): Decide how we would like to support float-based
  // decoding.
  absl::Status ConfigureBitDepth(OutputFileBitDepth bit_depth);

  /*!\brief Decodes the bitstream provided.
   *
   * Supports both descriptor OBUs, temporal units, and partial versions of
   * both. User can provide as much data as they would like. To receive decoded
   * temporal units, GetOutputTemporalUnit() should be called. If
   * GetOutputTemporalUnit() has not been called, this function guarantees that
   * any temporal units received thus far have not been lost. See sample usages
   * for more details.
   *
   * \param bitstream Bitstream to decode.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status Decode(absl::Span<const uint8_t> bitstream);

  /*!\brief Outputs the next temporal unit of decoded audio.
   *
   * If no decoded data is available, output_decoded_temporal_unit will be
   * empty. The user can continue calling until the output is empty, as there
   * may be more than one temporal unit available. When this returns empty, the
   * user should call Decode() again with more data.
   *
   * \param output_decoded_temporal_unit Output parameter for the next temporal
   *        unit of decoded audio.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetOutputTemporalUnit(
      std::vector<uint8_t>& output_decoded_temporal_unit);

  /*!\brief Returns true iff a decoded temporal unit is available.
   *
   * This function can be used to determine when the user should call
   * GetOutputTemporalUnit().
   *
   * \return true iff a decoded temporal unit is available.
   */
  bool IsTemporalUnitAvailable();

  /*!\brief Returns true iff the descriptor OBUs have been parsed.
   *
   * This function can be used for determining when configuration setters that
   * rely on descriptor OBU parsing can be called.
   *
   * \return true iff the descriptor OBUs have been parsed.
   */
  bool IsDescriptorProcessingComplete();

  /*!\brief Provides mix presentation information from the descriptor OBUs.
   *
   * This function can be used to determine which mix presentation the user
   * would like to configure the decoder with. It will fail if the descriptor
   * OBUs have not been parsed yet.
   *
   * \param output_mix_presentation_ids Output parameter for the mix
   *        presentation metadata.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetMixPresentations(
      std::vector<MixPresentationMetadata>& output_mix_presentation_ids);

  /*!\brief Gets the sample rate.
   *
   * \param output_sample_rate Output parameter for the sample rate.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetSampleRate(uint32_t& output_sample_rate);

  /*!\brief Gets the number of samples per frame.
   *
   * \param output_frame_size Output parameter for the frame size.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetFrameSize(uint32_t& output_frame_size);

  /*!\brief Outputs the last temporal unit(s) of decoded audio.
   *
   * Signals to the decoder that no more data will be provided; therefore it
   * should only be called once the user has finished providing data to
   * Decode(). Temporal units are output one at a time, so this function should
   * be called until output_is_done is true.
   *
   * \param output_decoded_temporal_unit Output parameter for the next temporal
   *        unit of decoded audio.
   * \param output_is_done Output parameter for whether there are more temporal
   *        units to be output.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status Flush(std::vector<uint8_t>& output_decoded_temporal_unit,
                     bool& output_is_done);

  /*!\brief Closes the decoder.
   *
   * This should be called once the user has finished providing data into
   * Decode() and has called Flush() until output_is_done is true. Will close
   * all underlying decoders.
   *
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status Close();

 private:
  /*!\brief Private constructor only used by Create functions.
   *
   * \param read_bit_buffer Read bit buffer to use for reading data. Expected to
   *        not be null.
   */
  IamfDecoder(std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer)
      : read_bit_buffer_(std::move(read_bit_buffer)) {}

  // Used to process descriptor OBUs and temporal units. Is only created after
  // the descriptor OBUs have been parsed.
  std::unique_ptr<ObuProcessor> obu_processor_;

  // Buffer that is filled with data from Decode().
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer_;

  // Rendered PCM samples. Each element in the outer vector corresponds to a
  // temporal unit. A temporal unit will never be partially filled, so the
  // number of elements in the outer vector is equal to the number of decoded
  // temporal units currently available.
  std::vector<std::vector<std::vector<int32_t>>> rendered_pcm_samples_;
};
}  // namespace iamf_tools

#endif  // API_IAMF_DECODER_H_
