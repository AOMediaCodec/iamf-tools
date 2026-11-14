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
#ifndef CLI_PROTO_CONVERSION_PROTO_IAMF_ENCODER_H_
#define CLI_PROTO_CONVERSION_PROTO_IAMF_ENCODER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/include/iamf_tools/iamf_encoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {

/*!\brief Adapts `IamfEncoder` to accept protocol buffer metadata inputs.
 *
 * Parses `UserMetadata` during construction and `IamfTemporalUnitData`
 * protocol buffers per temporal unit, delegating all encoding work to the
 * proto-free `IamfEncoder`.
 */
class ProtoIamfEncoder : public api::IamfEncoderInterface {
 public:
  /*!\brief Creates a `ProtoIamfEncoder` from `UserMetadata` protocol buffer.
   *
   * \param user_metadata Input user metadata describing the IAMF stream.
   * \param trimming_settings Trimming settings to configure the rendering,
   *        or `std::nullopt` to disable rendering.
   * \param loudness_calculator_factory Factory to create loudness calculators
   *        to measure the loudness of the output layouts.
   * \param sample_processor_factory Factory to create processors for use after
   *        rendering.
   * \param obu_sequencer_factory Factory to create `ObuSequencerBases`.
   * \return Encoder on success, or a specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<ProtoIamfEncoder>> Create(
      const iamf_tools_cli_proto::UserMetadata& user_metadata,
      std::optional<TrimmingSettings> trimming_settings,
      const LoudnessCalculatorFactoryBase* absl_nullable
          loudness_calculator_factory,
      const RenderingMixPresentationFinalizer::SampleProcessorFactory&
          sample_processor_factory,
      const IamfEncoder::ObuSequencerFactory& obu_sequencer_factory);

  /*!\brief Gets the latest descriptor OBUs.
   *
   * \param redundant_copy True to request a "redundant" copy.
   * \param descriptor_obus Serialized descriptor OBUs.
   * \param output_obus_are_finalized `true` when the output OBUs are
   *        finalized, `false` otherwise.
   * \return `api::IamfStatus::OkStatus()` if successful. A specific status on
   *         failure.
   */
  api::IamfStatus GetDescriptorObus(
      bool redundant_copy, std::vector<uint8_t>& descriptor_obus,
      bool& output_obus_are_finalized) const override;

  /*!\brief Gets the encoder delay in samples.
   *
   * \return Encoder delay in samples.
   */
  uint32_t GetEncoderDelay() const override;

  /*!\brief Gets the number of samples per frame required by the encoder.
   *
   * \return Number of samples per frame per channel.
   */
  uint32_t GetNumSamplesPerFrame() const override;

  /*!\brief Returns whether this encoder is generating data OBUs.
   *
   * \return True if still generating data OBUs.
   */
  bool GeneratingTemporalUnits() const override;

  /*!\brief Gets the input timestamp of the data OBU generation iteration.
   *
   * \param input_timestamp Output input timestamp.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status GetInputTimestamp(InternalTimestamp& input_timestamp);

  /*!\brief Adds audio data and serialized parameter block metadata for one TU.
   *
   * \param temporal_unit_data Temporal unit to add.
   * \return `api::IamfStatus::OkStatus()` if successful. A specific status on
   *         failure.
   */
  api::IamfStatus Encode(
      const api::IamfTemporalUnitData& temporal_unit_data) override;

  /*!\brief Adds planar audio data for one temporal unit of a single-element
   *        encoder.
   *
   * \param channel_samples Planar audio samples `[channel][sample]` in
   *        canonical channel order normalized to `[-1.0, 1.0]`.
   * \return `api::IamfStatus::OkStatus()` if successful. A specific status on
   *         failure.
   */
  api::IamfStatus Encode(
      std::span<const std::span<const double>> channel_samples) override;

  /*!\brief Outputs data OBUs corresponding to one temporal unit.
   *
   * \param temporal_unit_obus Output OBUs corresponding to this temporal unit.
   * \return `api::IamfStatus::OkStatus()` if successful. A specific status on
   *         failure.
   */
  api::IamfStatus OutputTemporalUnit(
      std::vector<uint8_t>& temporal_unit_obus) override;

  /*!\brief Finalizes the process of encoding.
   *
   * \return `api::IamfStatus::OkStatus()` if successful. A specific status on
   *         failure.
   */
  api::IamfStatus FinalizeEncode() override;

  /*!\brief Outputs a const reference to the Audio Elements.
   *
   * \return Const reference to the Audio Elements.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const DescriptorObus::AudioElementsById& GetAudioElements() const;

  /*!\brief Outputs a const reference to the Mix Presentation OBUs.
   *
   * \param output_is_finalized `true` when the output OBUs have been finalized.
   * \return Mix Presentation OBUs.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const DescriptorObus::MixPresentationObus& GetMixPresentationObus(
      bool& output_is_finalized) const;

  /*!\brief Outputs a const reference to the Descriptor Arbitrary OBUs.
   *
   * \return Const reference to the Descriptor Arbitrary OBUs.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const std::list<ArbitraryObu>& GetDescriptorArbitraryObus() const;

 private:
  explicit ProtoIamfEncoder(std::unique_ptr<IamfEncoder> encoder)
      : encoder_(std::move(encoder)) {}

  std::unique_ptr<IamfEncoder> encoder_;
  bool finalize_encode_called_ = false;
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_IAMF_ENCODER_H_
