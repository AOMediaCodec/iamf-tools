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
#ifndef OBU_AUDIO_ELEMENT_H_
#define OBU_AUDIO_ELEMENT_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief One of the parameters associated with an Audio Element OBU. */
struct AudioElementParam {
  friend bool operator==(const AudioElementParam& lhs,
                         const AudioElementParam& rhs) = default;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(uint32_t audio_element_id, ReadBitBuffer& rb);

  // One of the parameter definition subclasses allowed in an Audio Element.
  std::variant<DemixingParamDefinition, ReconGainParamDefinition,
               ExtendedParamDefinition>
      param_definition;

  /*!\brief Gets the actual type of parameter definition.
   *
   * \return Type of the stored parameter definition.
   */
  ParamDefinition::ParameterDefinitionType GetType() const {
    return std::visit(
        [](const auto& concrete_param_definition) {
          const auto param_definition_type =
              concrete_param_definition.GetType();

          // All alternatives have well-defined types.
          ABSL_CHECK(param_definition_type.has_value());
          return *param_definition_type;
        },
        param_definition);
  }
};

/*!\brief An element of the `ScalableChannelLayoutConfig` vector.
 *
 * Implements the `ChannelAudioLayerConfig` as defined by section 3.6.2 of
 * https://aomediacodec.github.io/iamf/v1.1.0.html.
 */
struct ChannelAudioLayerConfig {
  /*!\brief A 4-bit enum for the type of layout. */
  enum LoudspeakerLayout : uint8_t {
    kLayoutMono = 0,      // C.
    kLayoutStereo = 1,    // L/R
    kLayout5_1_ch = 2,    // L/C/R/Ls/Rs/LFE.
    kLayout5_1_2_ch = 3,  // L/C/R/Ls/Rs/Ltf/Rtf/LFE.
    kLayout5_1_4_ch = 4,  // L/C/R/Ls/Rs/Ltf/Rtf/Ltr/Rtr/LFE.
    kLayout7_1_ch = 5,    // L/C/R/Lss/Rss/Lrs/Rrs/LFE.
    kLayout7_1_2_ch = 6,  // L/C/R/Lss/Rss/Lrs/Rrs/Ltf/Rtf/LFE.
    kLayout7_1_4_ch = 7,  // L/C/R/Lss/Rss/Lrs/Rrs/Ltf/Rtf/Ltb/Rtb/LFE.
    kLayout3_1_2_ch = 8,  // L/C/R//Ltf/Rtf/LFE.
    kLayoutBinaural = 9,  // L/R.
    kLayoutReserved10 = 10,
    kLayoutReserved11 = 11,
    kLayoutReserved12 = 12,
    kLayoutReserved13 = 13,
    kLayoutReserved14 = 14,
    kLayoutExpanded = 15,
  };

  /*!\brief A 8-bit enum for the type of expanded layout. */
  enum ExpandedLoudspeakerLayout : uint8_t {
    kExpandedLayoutLFE = 0,      // Low-frequency effects subset (LFE) or 7.1.4.
    kExpandedLayoutStereoS = 1,  // Stereo subset (Ls/Rs) of 5.1.4.
    kExpandedLayoutStereoSS = 2,  // Side surround subset (Lss/Rss) of 7.1.4.
    kExpandedLayoutStereoRS = 3,  // Rear surround subset (Lrs/Rrs) of 7.1.4.
    kExpandedLayoutStereoTF = 4,  // Top front subset (Ltf/Rtf) of 7.1.4.
    kExpandedLayoutStereoTB = 5,  // Top back subset (Ltb/Rtb) of 7.1.4.
    kExpandedLayoutTop4Ch = 6,  // Top four channels (Ltf/Rtf/Ltb/Rtb) of 7.1.4.
    kExpandedLayout3_0_ch = 7,  // Front three channels (L/C/R) of 7.1.4.
    kExpandedLayout9_1_6_ch = 8,   // Subset of Sound System H [ITU-2051-3].
    kExpandedLayoutStereoF = 9,    // Front stereo subset (FL/FR) of 9.1.6.
    kExpandedLayoutStereoSi = 10,  // Side surround subset (SiL/SiR) of 9.1.6.
    kExpandedLayoutStereoTpSi =
        11,  // Top surround subset (TpSiL/TpSiR) of 9.1.6.
    kExpandedLayoutTop6Ch =
        12,  // Top six channels (TpFL/TpFR/TpSiL/TpSiR/TpBL/TpBR) of 9.1.6.
    kExpandedLayoutReserved13 = 13,
    kExpandedLayoutReserved255 = 255,
  };

  friend bool operator==(const ChannelAudioLayerConfig& lhs,
                         const ChannelAudioLayerConfig& rhs) = default;

  /*!\brief Writes the `ChannelAudioLayerConfig` payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Reads the `ChannelAudioLayerConfig` payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Read(ReadBitBuffer& rb);

  LoudspeakerLayout loudspeaker_layout;  // 4 bits.
  bool output_gain_is_present_flag;
  bool recon_gain_is_present_flag;
  uint8_t reserved_a;  // 2 bits.
  uint8_t substream_count;
  uint8_t coupled_substream_count;

  // if (output_gain_is_present_flag(i)) {
  uint8_t output_gain_flag = 0;  // 6 bits.
  uint8_t reserved_b = 0;        // 2 bits.
  int16_t output_gain = 0;
  // }

  // if (loudspeaker_layout == kLayoutExpanded) {
  std::optional<ExpandedLoudspeakerLayout> expanded_loudspeaker_layout;
  // }
};

/*!\brief Config to reconstruct an Audio Element OBU using a channel layout.
 *
 * The metadata required for combining the substreams identified here in order
 * to reconstruct a scalable channel layout.
 */
struct ScalableChannelLayoutConfig {
  friend bool operator==(const ScalableChannelLayoutConfig& lhs,
                         const ScalableChannelLayoutConfig& rhs) = default;

  /*!\brief Validates the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate(DecodedUleb128 num_substreams_in_audio_element) const;

  /*!\brief Gets the number of layers in the configuration.
   *
   * \return Number of layers.
   */
  uint8_t GetNumLayers() const { return channel_audio_layer_configs.size(); }

  uint8_t reserved = 0;  // 5 bits.

  // Vector of layers.
  std::vector<ChannelAudioLayerConfig> channel_audio_layer_configs;
};

/*!\brief Configuration for object-based audio elements. */
struct ObjectsConfig {
  friend bool operator==(const ObjectsConfig& lhs,
                         const ObjectsConfig& rhs) = default;

  /*!\brief Move constructor. */
  ObjectsConfig(ObjectsConfig&& other) = default;

  /*!\brief Copy constructor. */
  ObjectsConfig(const ObjectsConfig& other) = default;

  /*!\brief Copy assignment operator. */
  ObjectsConfig& operator=(const ObjectsConfig& other) = default;

  /*!\brief Creates an `ObjectsConfig` from a buffer.
   *
   * \param rb Buffer to read from.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  static absl::StatusOr<ObjectsConfig> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Creates an `ObjectsConfig` from the given parameters.
   *
   * \param num_objects Number of objects.
   * \param objects_config_extension_bytes Objects config extension bytes.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  static absl::StatusOr<ObjectsConfig> Create(
      uint8_t num_objects,
      absl::Span<const uint8_t> objects_config_extension_bytes);

  const uint8_t num_objects;  // 8 bits.

  const std::vector<uint8_t> objects_config_extension_bytes;

 private:
  // Private constructor. Use `Create` or `CreateFromBuffer` instead.
  ObjectsConfig(uint8_t num_objects,
                std::vector<uint8_t> objects_config_extension_bytes);
};

/*!\brief Configuration for mono-coded Ambisonics. */
struct AmbisonicsMonoConfig {
  // RFC 8486 reserves 255 to signal an inactive ACN (ambisonics channel
  // number).
  static constexpr uint8_t kInactiveAmbisonicsChannelNumber = 255;

  friend bool operator==(const AmbisonicsMonoConfig& lhs,
                         const AmbisonicsMonoConfig& rhs) = default;

  /*!\brief Validates the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate(DecodedUleb128 num_substreams_in_audio_element) const;

  uint8_t output_channel_count;  // (C).
  uint8_t substream_count;       // (N).

  // Vector of length (C).
  std::vector<uint8_t> channel_mapping;
};

/*!\brief Configuration for projection-coded Ambisonics. */
struct AmbisonicsProjectionConfig {
  friend bool operator==(const AmbisonicsProjectionConfig& lhs,
                         const AmbisonicsProjectionConfig& rhs) = default;

  /*!\brief Validates the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate(DecodedUleb128 num_substreams_in_audio_element) const;

  uint8_t output_channel_count;     // (C).
  uint8_t substream_count;          // (N).
  uint8_t coupled_substream_count;  // (M).

  // Vector of length (N + M) * C.
  std::vector<int16_t> demixing_matrix;
};

/*!\brief Config to reconstruct an Audio Element OBU using Ambisonics layout.
 *
 * The metadata required for combining the substreams identified here in order
 * to reconstruct an Ambisonics layout.
 */
struct AmbisonicsConfig {
  /*!\brief A `DecodedUleb128` enum for the method of coding Ambisonics. */
  enum AmbisonicsMode : DecodedUleb128 {
    kAmbisonicsModeMono = 0,
    kAmbisonicsModeProjection = 1,
    kAmbisonicsModeReservedStart = 2,
    kAmbisonicsModeReservedEnd = std::numeric_limits<DecodedUleb128>::max(),
  };
  friend bool operator==(const AmbisonicsConfig& lhs,
                         const AmbisonicsConfig& rhs) = default;

  /*!\brief Gets the next valid number of output channels.
   *
   * \param requested_output_channel_count Requested number of channels.
   * \param next_valid_output_channel_count Minimum valid `output_channel_count`
   *        that has at least the required number of channels.
   * \return `absl::OkStatus()` if successful. `kIamfInvalid` argument if
   *         the input is too large.
   */
  static absl::Status GetNextValidOutputChannelCount(
      uint8_t requested_output_channel_count,
      uint8_t& next_valid_output_channel_count);

  AmbisonicsMode ambisonics_mode;  // Serialized to a ULEB128.

  // The active field depends on `ambisonics_mode`.
  std::variant<AmbisonicsMonoConfig, AmbisonicsProjectionConfig>
      ambisonics_config;
};

struct ExtensionConfig {
  friend bool operator==(const ExtensionConfig& lhs,
                         const ExtensionConfig& rhs) = default;

  // `audio_element_config_size` is inferred from the size of
  // `audio_element_config_bytes`.
  std::vector<uint8_t> audio_element_config_bytes;
};

/*!\brief Audio Element OBU.
 *
 * Create the audio element, and optionally initialize the parameters.
 * 1. `InitializeParams()`.
 *
 * This class has stricter limits than the specification:
 *   - Maximum number parameters is limited to `kMaxNumParameters`.
 */
class AudioElementObu : public ObuBase {
 public:
  /*!\brief Artificial limit on the maximum number of parameters. */
  static constexpr uint32_t kMaxNumParameters = 256;

  /*!\brief A 3-bit enum for the type of Audio Element. */
  enum AudioElementType : uint8_t {
    kAudioElementChannelBased = 0,
    kAudioElementSceneBased = 1,
    kAudioElementObjectBased = 2,
    // Values in the range of [3 - 7] are reserved.
    kAudioElementBeginReserved = 3,
    kAudioElementEndReserved = 7,
  };

  typedef std::variant<ScalableChannelLayoutConfig, AmbisonicsConfig,
                       ObjectsConfig, ExtensionConfig>
      AudioElementConfig;

  /*!brief Creates a `AudioElementObu` for a scalable channel layout.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_ids IDs of the substreams in the audio element.
   * \param scalable_channel_layout_config Configuration of the audio element.
   * \return `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateForScalableChannelLayout(
      const ObuHeader& header, DecodedUleb128 audio_element_id,
      uint8_t reserved, DecodedUleb128 codec_config_id,
      absl::Span<const DecodedUleb128> audio_substream_ids,
      const ScalableChannelLayoutConfig& scalable_channel_layout_config);

  /*!brief Creates a `AudioElementObu` for a mono Ambisonics layout.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_ids IDs of the substreams in the audio element.
   * \param channel_mapping Channel mapping of the audio element.
   * \return `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateForMonoAmbisonics(
      const ObuHeader& header, DecodedUleb128 audio_element_id,
      uint8_t reserved, DecodedUleb128 codec_config_id,
      absl::Span<const DecodedUleb128> audio_substream_ids,
      absl::Span<const uint8_t> channel_mapping);

  /*!brief Creates a `AudioElementObu` for a projection Ambisonics layout.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_ids IDs of the substreams in the audio element.
   * \param output_channel_count Number of output channels.
   * \param coupled_substream_count Number of coupled substreams.
   * \param demixing_matrix Demixing matrix of the audio element.
   * \return `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateForProjectionAmbisonics(
      const ObuHeader& header, DecodedUleb128 audio_element_id,
      uint8_t reserved, DecodedUleb128 codec_config_id,
      absl::Span<const DecodedUleb128> audio_substream_ids,
      uint8_t output_channel_count, uint8_t coupled_substream_count,
      absl::Span<const int16_t> demixing_matrix);

  /*!brief Creates a `AudioElementObu` for objects.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_id IDs of the substream in the audio element.
   * \param objects_config Configuration of the audio element.
   * \return `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateForObjects(
      const ObuHeader& header, DecodedUleb128 audio_element_id,
      uint8_t reserved, DecodedUleb128 codec_config_id,
      DecodedUleb128 audio_substream_id, const ObjectsConfig& objects_config);

  /*!brief Creates a `AudioElementObu` for an extension.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_ids IDs of the substreams in the audio element.
   * \param audio_element_config_bytes Audio element config bytes of the audio
   *        element.
   * \return `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateForExtension(
      const ObuHeader& header, DecodedUleb128 audio_element_id,
      AudioElementType audio_element_type, uint8_t reserved,
      DecodedUleb128 codec_config_id,
      absl::Span<const DecodedUleb128> audio_substream_ids,
      absl::Span<const uint8_t> audio_element_config_bytes);

  /*!\brief Creates a `AudioElementObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `AudioElementObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return an `AudioElementObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioElementObu> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb);

  /*!\brief Copy constructor.*/
  AudioElementObu(const AudioElementObu& other) = default;

  /*!\brief Move constructor.*/
  AudioElementObu(AudioElementObu&& other) = default;

  /*!\brief Destructor. */
  ~AudioElementObu() override = default;

  friend bool operator==(const AudioElementObu& lhs,
                         const AudioElementObu& rhs) = default;

  /*!\brief Initializes the `audio_element_params_` vector.
   *
   * \param num_parameters Number of parameters.
   */
  void InitializeParams(uint32_t num_parameters);

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*!\brief Gets the type of the audio element.
   *
   * \return Type of the audio element.
   */
  AudioElementType GetAudioElementType() const { return audio_element_type_; }

  /*!\brief Gets the audio element ID.
   *
   * \return Audio element ID.
   */
  DecodedUleb128 GetAudioElementId() const { return audio_element_id_; }

  /*!\brief Gets the codec config ID associated with the audio element.
   *
   * \return Codec config ID associated with the audio element.
   */
  DecodedUleb128 GetCodecConfigId() const { return codec_config_id_; }

  /*!\brief Gets the number of substreams in the audio element.
   *
   * \return Number of substreams in the audio element.
   */
  DecodedUleb128 GetNumSubstreams() const {
    return audio_substream_ids_.size();
  }

  /*!\brief Gets the number of parameters in the audio element.
   *
   * \return Number of parameters in the audio element.
   */
  DecodedUleb128 GetNumParameters() const {
    return audio_element_params_.size();
  }

  // Vector of substream IDs.
  std::vector<DecodedUleb128> audio_substream_ids_;

  // Vector of audio element parameters.
  std::vector<AudioElementParam> audio_element_params_;

  // Active field depends on `audio_element_type_`.
  AudioElementConfig config_;

 private:
  DecodedUleb128 audio_element_id_;
  AudioElementType audio_element_type_;  // 3 bits.
  uint8_t reserved_ = 0;                 // 5 bits.

  // ID of the associated Codec Config OBU.
  DecodedUleb128 codec_config_id_;

  // Used only by the factory create function.
  explicit AudioElementObu(const ObuHeader& header)
      : ObuBase(header, kObuIaAudioElement),
        audio_element_id_(DecodedUleb128()),
        audio_element_type_(kAudioElementBeginReserved),
        codec_config_id_(DecodedUleb128()) {}

  /*!\brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param audio_element_id ID of the audio element.
   * \param audio_element_type Type of the audio element.
   * \param reserved Reserved field.
   * \param codec_config_id ID of the associated codec config.
   * \param audio_substream_ids IDs of the substreams in the audio element.
   * \param config Configuration of the audio element.
   */
  AudioElementObu(const ObuHeader& header, DecodedUleb128 audio_element_id,
                  AudioElementType audio_element_type, uint8_t reserved,
                  DecodedUleb128 codec_config_id,
                  absl::Span<const DecodedUleb128> audio_substream_ids,
                  const AudioElementConfig& config);

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // OBU_AUDIO_ELEMENT_H_
