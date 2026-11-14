/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_PROTO_CONVERSION_PROTO_TO_OBU_AUDIO_FRAME_GENERATOR_FACTORY_H_
#define CLI_PROTO_CONVERSION_PROTO_TO_OBU_AUDIO_FRAME_GENERATOR_FACTORY_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_frame_generator.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/codec_config.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

/*!\brief Creates an `AudioFrameGenerator` from protocol buffer metadata.
 *
 * Validates protocol buffer metadata, initializes underlying codec encoders,
 * converts channel labels and trimming configuration, and delegates to the
 * proto-free `AudioFrameGenerator::Create`.
 *
 * \param audio_frame_metadatas Input audio frame metadata protos.
 * \param codec_config_metadatas Input codec config metadata protos.
 * \param audio_elements Audio elements keyed by ID.
 * \param downmixer_manager Downmixer manager to transfer ownership to the
 *        generator.
 * \param parameters_manager Manager of parameters.
 * \param global_timing_module Global Timing Module.
 * \return Unique pointer to an `AudioFrameGenerator` on success. A specific
 *         status on failure.
 */
absl::StatusOr<std::unique_ptr<AudioFrameGenerator> absl_nonnull>
CreateAudioFrameGeneratorFromMetadata(
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadatas,
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadatas,
    const DescriptorObus::AudioElementsById& audio_elements,
    std::unique_ptr<DownmixerManager> absl_nonnull downmixer_manager,
    ParametersManager& parameters_manager,
    GlobalTimingModule& global_timing_module);

/*!\brief Gets the number of samples to delay at the start for a codec.
 *
 * Useful to determine the `codec_delay` field when initializing the encoder.
 *
 * \param codec_config_metadata Input codec config metadata proto.
 * \param codec_config Codec Config OBU to configure the encoder with.
 * \return Number of samples to delay at the start on success. A specific
 *         status on failure.
 */
absl::StatusOr<uint32_t> GetNumberOfSamplesToDelayAtStart(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config);

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_TO_OBU_AUDIO_FRAME_GENERATOR_FACTORY_H_
