# Example commandline interface tools for the IAMF software.

package(default_visibility = ["//iamf/cli:__subpackages__"])

exports_files(
    srcs = [
        "encoder_main.cc",
        "encoder_main_lib.cc",
        "encoder_main_lib.h",
        "iamf_components.h",
    ],
)

cc_library(
    name = "arbitrary_obu_generator",
    srcs = ["arbitrary_obu_generator.cc"],
    hdrs = ["arbitrary_obu_generator.h"],
    deps = [
        ":cli_util",
        "//iamf/cli/proto:arbitrary_obu_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/base:no_destructor",
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
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:leb128",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "@com_google_absl//absl/base:no_destructor",
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
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:leb128",
        "//iamf/obu:parameter_block",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "audio_frame_decoder",
    srcs = ["audio_frame_decoder.cc"],
    hdrs = ["audio_frame_decoder.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":cli_util",
        ":wav_writer",
        "//iamf/cli/codec:aac_encoder_decoder",
        "//iamf/cli/codec:decoder_base",
        "//iamf/cli/codec:opus_encoder_decoder",
        "//iamf/common:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu:demixing_info_param_data",
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
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":demixing_module",
        ":global_timing_module",
        ":parameters_manager",
        "//iamf/cli/codec:aac_encoder_decoder",
        "//iamf/cli/codec:encoder_base",
        "//iamf/cli/codec:flac_encoder_decoder",
        "//iamf/cli/codec:lpcm_encoder",
        "//iamf/cli/codec:opus_encoder_decoder",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "audio_frame_with_data",
    hdrs = ["audio_frame_with_data.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/obu:audio_frame",
        "//iamf/obu:demixing_info_param_data",
    ],
)

cc_library(
    name = "cli_util",
    srcs = ["cli_util.cc"],
    hdrs = ["cli_util.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
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
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "demixing_module",
    srcs = ["demixing_module.cc"],
    hdrs = ["demixing_module.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_decoder",
        ":audio_frame_with_data",
        ":cli_util",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
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
        ":parameter_block_partitioner",
        ":parameter_block_with_data",
        ":parameters_manager",
        ":wav_sample_provider",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "global_timing_module",
    srcs = ["global_timing_module.cc"],
    hdrs = ["global_timing_module.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:leb128",
        "//iamf/obu:param_definitions",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "ia_sequence_header_generator",
    srcs = ["ia_sequence_header_generator.cc"],
    hdrs = ["ia_sequence_header_generator.h"],
    deps = [
        ":cli_util",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/common:macros",
        "//iamf/obu:ia_sequence_header",
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
    visibility = ["//iamf:__subpackages__"],
    deps = [
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/obu:leb128",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "loudness_calculator",
    srcs = ["loudness_calculator.cc"],
    hdrs = ["loudness_calculator.h"],
    deps = [
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "loudness_calculator_factory",
    srcs = ["loudness_calculator_factory.cc"],
    hdrs = ["loudness_calculator_factory.h"],
    deps = [
        ":loudness_calculator",
        "//iamf/obu:mix_presentation",
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
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:mix_presentation",
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
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "@com_google_absl//absl/base:no_destructor",
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
        "//iamf/common:macros",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_block",
        "//iamf/obu:temporal_delimiter",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
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
        ":parameter_block_with_data",
        ":recon_gain_generator",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
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
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parameter_block_with_data",
    hdrs = ["parameter_block_with_data.h"],
    deps = ["//iamf/obu:parameter_block"],
)

cc_library(
    name = "parameters_manager",
    srcs = ["parameters_manager.cc"],
    hdrs = ["parameters_manager.h"],
    deps = [
        ":audio_element_with_data",
        ":parameter_block_with_data",
        "//iamf/common:macros",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "//iamf/obu:param_definitions",
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
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "renderer_factory",
    srcs = ["renderer_factory.cc"],
    hdrs = ["renderer_factory.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/obu:audio_element",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
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
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/obu:leb128",
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
        "//iamf/cli/adm_to_user_metadata/app:adm_to_user_metadata_main_lib",
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
