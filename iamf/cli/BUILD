# Example commandline interface tools for the IAMF software.

package(default_visibility = ["//:__subpackages__"])

exports_files(
    srcs = [
        "encoder_main.cc",
        "encoder_main_lib.cc",
        "encoder_main_lib.h",
        "iamf_components.h",
    ],
)

cc_library(
    name = "aac_encoder_decoder",
    srcs = ["aac_encoder_decoder.cc"],
    hdrs = ["aac_encoder_decoder.h"],
    deps = [
        ":audio_frame_with_data",
        ":decoder_base",
        ":encoder_base",
        "//iamf:aac_decoder_config",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf:write_bit_buffer",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@fdk_aac//:aac_decoder_lib",
        "@fdk_aac//:aac_encoder_lib",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "arbitrary_obu_generator",
    srcs = ["arbitrary_obu_generator.cc"],
    hdrs = ["arbitrary_obu_generator.h"],
    deps = [
        ":cli_util",
        "//iamf:arbitrary_obu",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf/cli/proto:arbitrary_obu_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "audio_element_generator",
    srcs = ["audio_element_generator.cc"],
    hdrs = ["audio_element_generator.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "audio_element_with_data",
    hdrs = ["audio_element_with_data.h"],
    deps = [
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:parameter_block",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "audio_frame_decoder",
    srcs = ["audio_frame_decoder.cc"],
    hdrs = ["audio_frame_decoder.h"],
    deps = [
        ":aac_encoder_decoder",
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":cli_util",
        ":decoder_base",
        ":opus_encoder_decoder",
        ":wav_writer",
        "//iamf:codec_config",
        "//iamf:ia",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_frame_generator",
    srcs = ["audio_frame_generator.cc"],
    hdrs = ["audio_frame_generator.h"],
    deps = [
        ":aac_encoder_decoder",
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":demixing_module",
        ":encoder_base",
        ":flac_encoder_decoder",
        ":global_timing_module",
        ":lpcm_encoder",
        ":opus_encoder_decoder",
        ":parameters_manager",
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:write_bit_buffer",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "audio_frame_with_data",
    hdrs = ["audio_frame_with_data.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf:audio_frame",
        "//iamf:demixing_info_param_data",
    ],
)

cc_library(
    name = "cli_util",
    srcs = ["cli_util.cc"],
    hdrs = ["cli_util.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_header",
        "//iamf:obu_util",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "codec_config_generator",
    srcs = ["codec_config_generator.cc"],
    hdrs = ["codec_config_generator.h"],
    deps = [
        ":cli_util",
        "//iamf:aac_decoder_config",
        "//iamf:codec_config",
        "//iamf:flac_decoder_config",
        "//iamf:ia",
        "//iamf:lpcm_decoder_config",
        "//iamf:obu_util",
        "//iamf:opus_decoder_config",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "decoder_base",
    hdrs = ["decoder_base.h"],
    deps = ["@com_google_absl//absl/status"],
)

cc_library(
    name = "demixing_module",
    srcs = ["demixing_module.cc"],
    hdrs = ["demixing_module.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_decoder",
        ":audio_frame_with_data",
        "//iamf:audio_frame",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "encoder_base",
    srcs = ["encoder_base.cc"],
    hdrs = ["encoder_base.h"],
    deps = [
        ":audio_frame_with_data",
        "//iamf:codec_config",
        "//iamf:ia",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "encoder_main_lib",
    srcs = ["encoder_main_lib.cc"],
    hdrs = ["encoder_main_lib.h"],
    deps = [
        ":arbitrary_obu_generator",
        ":audio_element_generator",
        ":audio_element_with_data",
        ":audio_frame_decoder",
        ":audio_frame_generator",
        ":audio_frame_with_data",
        ":cli_util",
        ":codec_config_generator",
        ":demixing_module",
        ":global_timing_module",
        ":ia_sequence_header_generator",
        ":iamf_components",
        ":mix_presentation_generator",
        ":obu_sequencer",
        ":parameter_block_generator",
        ":parameter_block_with_data",
        ":parameters_manager",
        ":wav_sample_provider",
        "//iamf:arbitrary_obu",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "flac_encoder_decoder",
    srcs = ["flac_encoder_decoder.cc"],
    hdrs = ["flac_encoder_decoder.h"],
    deps = [
        ":audio_frame_with_data",
        ":encoder_base",
        "//iamf:codec_config",
        "//iamf:flac_decoder_config",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@flac//:src",
    ],
)

cc_library(
    name = "global_timing_module",
    srcs = ["global_timing_module.cc"],
    hdrs = ["global_timing_module.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "ia_sequence_header_generator",
    srcs = ["ia_sequence_header_generator.cc"],
    hdrs = ["ia_sequence_header_generator.h"],
    deps = [
        ":cli_util",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "iamf_components",
    srcs = ["iamf_components.cc"],
    hdrs = ["iamf_components.h"],
    deps = [
        ":leb_generator",
        ":mix_presentation_finalizer",
        ":obu_sequencer",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "leb_generator",
    srcs = ["leb_generator.cc"],
    hdrs = ["leb_generator.h"],
    deps = [
        "//iamf:ia",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "lpcm_encoder",
    srcs = ["lpcm_encoder.cc"],
    hdrs = ["lpcm_encoder.h"],
    deps = [
        ":audio_frame_with_data",
        ":cli_util",
        ":encoder_base",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:lpcm_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "mix_presentation_finalizer",
    srcs = ["mix_presentation_finalizer.cc"],
    hdrs = ["mix_presentation_finalizer.h"],
    deps = [
        ":audio_element_with_data",
        ":demixing_module",
        ":mix_presentation_generator",
        ":parameter_block_with_data",
        "//iamf:ia",
        "//iamf:mix_presentation",
        "//iamf:obu_util",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "mix_presentation_generator",
    srcs = ["mix_presentation_generator.cc"],
    hdrs = ["mix_presentation_generator.h"],
    deps = [
        ":cli_util",
        "//iamf:ia",
        "//iamf:mix_presentation",
        "//iamf:obu_util",
        "//iamf:param_definitions",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "obu_sequencer",
    srcs = ["obu_sequencer.cc"],
    hdrs = ["obu_sequencer.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":leb_generator",
        ":parameter_block_with_data",
        "//iamf:arbitrary_obu",
        "//iamf:audio_element",
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_header",
        "//iamf:parameter_block",
        "//iamf:temporal_delimiter",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "opus_encoder_decoder",
    srcs = ["opus_encoder_decoder.cc"],
    hdrs = ["opus_encoder_decoder.h"],
    deps = [
        ":audio_frame_with_data",
        ":decoder_base",
        ":encoder_base",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:obu_util",
        "//iamf:opus_decoder_config",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@libopus",
    ],
)

cc_library(
    name = "parameter_block_generator",
    srcs = ["parameter_block_generator.cc"],
    hdrs = ["parameter_block_generator.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        ":demixing_module",
        ":global_timing_module",
        ":parameter_block_partitioner",
        ":parameter_block_with_data",
        ":recon_gain_generator",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_util",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "parameter_block_partitioner",
    srcs = ["parameter_block_partitioner.cc"],
    hdrs = ["parameter_block_partitioner.h"],
    deps = [
        ":cli_util",
        ":parameter_block_with_data",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parameter_block_with_data",
    hdrs = ["parameter_block_with_data.h"],
    deps = ["//iamf:parameter_block"],
)

cc_library(
    name = "parameters_manager",
    srcs = ["parameters_manager.cc"],
    hdrs = ["parameters_manager.h"],
    deps = [
        ":audio_element_with_data",
        ":parameter_block_with_data",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:param_definitions",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "recon_gain_generator",
    srcs = ["recon_gain_generator.cc"],
    hdrs = ["recon_gain_generator.h"],
    deps = [
        ":demixing_module",
        "//iamf:ia",
        "//iamf:obu_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "wav_reader",
    srcs = ["wav_reader.cc"],
    hdrs = ["wav_reader.h"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_audio_to_tactile//:dsp",
    ],
)

cc_library(
    name = "wav_sample_provider",
    srcs = ["wav_sample_provider.cc"],
    hdrs = ["wav_sample_provider.h"],
    deps = [
        ":audio_element_with_data",
        ":demixing_module",
        ":wav_reader",
        "//iamf:ia",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "wav_writer",
    srcs = ["wav_writer.cc"],
    hdrs = ["wav_writer.h"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_audio_to_tactile//:dsp",
    ],
)

cc_binary(
    name = "encoder_main",
    srcs = [
        "encoder_main.cc",
    ],
    data = [
        "//iamf/cli/testdata:Mechanism_5s.wav",
        "//iamf/cli/testdata:Mechanism_5s_32bit.wav",
        "//iamf/cli/testdata:Transport_TOA_5s.wav",
        "//iamf/cli/testdata:audiolab-acoustic-guitar_2OA_470_ALLRAD_0.5s.wav",
        "//iamf/cli/testdata:audiolab-acoustic-guitar_2OA_470_ALLRAD_5s.wav",
        "//iamf/cli/testdata:audiolab-acoustic-guitar_2OA_470_ALLRAD_concat.wav",
        "//iamf/cli/testdata:dialog_clip_stereo.wav",
        "//iamf/cli/testdata:sample1_48kHz_stereo.wav",
        "//iamf/cli/testdata:sawtooth_10000_foa_48khz.wav",
        "//iamf/cli/testdata:sawtooth_10000_stereo_44100hz_s16le.wav",
        "//iamf/cli/testdata:sawtooth_10000_stereo_48khz.wav",
        "//iamf/cli/testdata:sawtooth_10000_stereo_48khz_s24le.wav",
        "//iamf/cli/testdata:sawtooth_100_stereo.wav",
        "//iamf/cli/testdata:sawtooth_8000_toa_48khz.wav",
        "//iamf/cli/testdata:sine_1000_16khz_512ms.wav",
        "//iamf/cli/testdata:sine_1000_16khz_512ms_s32le.wav",
        "//iamf/cli/testdata:sine_1000_48khz.wav",
        "//iamf/cli/testdata:sine_1000_48khz_512ms.wav",
    ],
    deps = [
        ":encoder_main_lib",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)
