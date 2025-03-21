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

#ifndef API_DECODER_IAMF_DECODER_H_
#define API_DECODER_IAMF_DECODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/api/types.h"

namespace iamf_tools {
namespace api {

/*!brief The class and entrypoint for decoding IAMF bitstreams. */
class IamfDecoder {
 public:
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

  // Dtor cannot be inline (so it must be declared and defined in the source
  // file) because this class holds a (unique) pointer to the partial class,
  // DecoderState.  Moves must be declared and defined because dtor is defined.
  ~IamfDecoder();
  IamfDecoder(IamfDecoder&&);
  IamfDecoder& operator=(IamfDecoder&&);

  /*!\brief Creates an IamfDecoder.
   *
   * This function should be used for pure streaming applications in which the
   * descriptor OBUs are not known in advance.
   *
   * \param requested_layout Specifies the desired output layout. This layout
   *        will be used so long as it is present in the Descriptor OBUs that
   *        are later provided to Decode(). If not, a default layout will be
   *        selected.
   *
   * \return IamfDecoder upon success. Other specific statuses on
   *         failure.
   */
  static absl::StatusOr<IamfDecoder> Create(
      const OutputLayout& requested_layout);

  /*!\brief Creates an IamfDecoder from a known set of descriptor OBUs.
   *
   * This function should be used for applications in which the descriptor OBUs
   * are known in advance.
   *
   * \param requested_layout Specifies the desired output layout. This layout
   *        will be used so long as it is present in the Descriptor OBUs that
   *        are provided. If not, a default layout will be selected.
   * \param descriptor_obus Bitstream containing all the descriptor OBUs and
   *        only descriptor OBUs.
   * \return IamfDecoder upon success. Other specific statuses on
   *         failure.
   */
  static absl::StatusOr<IamfDecoder> CreateFromDescriptors(
      const OutputLayout& requested_layout,
      absl::Span<const uint8_t> descriptor_obus);

  /*!\brief Configures the decoder with the desired mix presentation.
   *
   * \param mix_presentation_id Specifies the desired mix presentation.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status ConfigureMixPresentationId(
      MixPresentationId mix_presentation_id);

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
   * any temporal units received thus far have not been lost. If descriptors are
   * processed for the first time, function will exit before processing any
   * temporal units. This provides the user a chance to configure the decoder as
   * they see fit. See sample usages for more details.
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
   *        unit of decoded audio. The outer vector corresponds to a tick, while
   *        the inner vector corresponds to a channel.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetOutputTemporalUnit(
      std::vector<std::vector<int32_t>>& output_decoded_temporal_unit);

  /*!\brief Returns true iff a decoded temporal unit is available.
   *
   * This function can be used to determine when the user should call
   * GetOutputTemporalUnit().
   *
   * \return true iff a decoded temporal unit is available.
   */
  bool IsTemporalUnitAvailable() const;

  /*!\brief Returns true iff the descriptor OBUs have been parsed.
   *
   * This function can be used for determining when configuration setters that
   * rely on Descriptor OBU parsing can be called.
   *
   * \return true iff the Descriptor OBUs have been parsed.
   */
  bool IsDescriptorProcessingComplete() const;

  /*!\brief Gets the layout that will be used to render the audio.
   *
   * The actual Layout used for rendering may not the same as requested when
   * creating the IamfDecoder, if the requested Layout could not be used.
   * This function allows verifying the actual Layout used after Descriptor OBU
   * parsing is complete.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \return OutputLayout or error statuses on failure.
   */
  absl::StatusOr<OutputLayout> GetOutputLayout() const;

  /*!\brief Gets the number of output channels.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \return
   */
  absl::StatusOr<int> GetNumberOfOutputChannels() const;

  /*!\brief Provides mix presentation information from the descriptor OBUs.
   *
   * This function can be used to determine which mix presentation the user
   * would like to configure the decoder with.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_mix_presentation_metadatas Output parameter for the mix
   *        presentation metadata.
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::Status GetMixPresentations(std::vector<MixPresentationMetadata>&
                                       output_mix_presentation_metadatas) const;

  /*!\brief Gets the sample rate.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \return `absl::OkStatus()` upon success. Other specific statuses on
   *         failure.
   */
  absl::StatusOr<uint32_t> GetSampleRate() const;

  /*!\brief Gets the number of samples per frame.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * Returns the number of samples per frame of the output audio. The total
   * number of samples in a time tick is the number of channels times the number
   * of samples per frame.
   *
   * \return Number of samples per frame upon success. Other specific statuses
   *         on failure.
   */
  absl::StatusOr<uint32_t> GetFrameSize() const;

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
  absl::Status Flush(
      std::vector<std::vector<int32_t>>& output_decoded_temporal_unit,
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
  // Forward declaration of the internal state of the decoder.
  struct DecoderState;

  // Private constructor only used by Create functions.
  IamfDecoder(std::unique_ptr<DecoderState> state);

  // Internal state of the decoder.
  std::unique_ptr<DecoderState> state_;
};
}  // namespace api
}  // namespace iamf_tools

#endif  // API_DECODER_IAMF_DECODER_H_
