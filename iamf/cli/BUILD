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
    name = "audio_element_with_data",
    hdrs = ["audio_element_with_data.h"],
    deps = [
        ":channel_label",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:parameter_block",
        "//iamf/obu:types",
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
        "//iamf/cli/codec:aac_decoder",
        "//iamf/cli/codec:decoder_base",
        "//iamf/cli/codec:lpcm_decoder",
        "//iamf/cli/codec:opus_decoder",
        "//iamf/common:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu:parameter_data",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_frame_with_data",
    hdrs = ["audio_frame_with_data.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/obu:audio_frame",
        "//iamf/obu:parameter_data",
    ],
)

cc_library(
    name = "channel_label",
    srcs = ["channel_label.cc"],
    hdrs = ["channel_label.h"],
    deps = [
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:parameter_data",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "cli_util",
    srcs = ["cli_util.cc"],
    hdrs = ["cli_util.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
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
        ":channel_label",
        ":cli_util",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
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
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":cli_util",
        ":demixing_module",
        ":iamf_components",
        ":iamf_encoder",
        ":obu_sequencer",
        ":parameter_block_partitioner",
        ":parameter_block_with_data",
        ":wav_sample_provider",
        ":wav_writer",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_to_obu:arbitrary_obu_generator",
        "//iamf/common:macros",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
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
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
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
    ],
)

cc_library(
    name = "iamf_encoder",
    srcs = ["iamf_encoder.cc"],
    hdrs = ["iamf_encoder.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_decoder",
        ":audio_frame_with_data",
        ":channel_label",
        ":cli_util",
        ":demixing_module",
        ":global_timing_module",
        ":parameter_block_with_data",
        ":parameters_manager",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_to_obu:audio_element_generator",
        "//iamf/cli/proto_to_obu:audio_frame_generator",
        "//iamf/cli/proto_to_obu:codec_config_generator",
        "//iamf/cli/proto_to_obu:ia_sequence_header_generator",
        "//iamf/cli/proto_to_obu:mix_presentation_generator",
        "//iamf/cli/proto_to_obu:parameter_block_generator",
        "//iamf/common:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
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
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
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
        ":parameter_block_with_data",
        ":wav_writer",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
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
        ":profile_filter",
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
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
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
        "//iamf/obu:demixing_param_definition",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "profile_filter",
    srcs = ["profile_filter.cc"],
    hdrs = ["profile_filter.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "recon_gain_generator",
    srcs = ["recon_gain_generator.cc"],
    hdrs = ["recon_gain_generator.h"],
    deps = [
        ":channel_label",
        ":demixing_module",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:types",
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
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
    ],
)

cc_library(
    name = "wav_reader",
    srcs = ["wav_reader.cc"],
    hdrs = ["wav_reader.h"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_audio_to_tactile//:dsp",
    ],
)

cc_library(
    name = "wav_sample_provider",
    srcs = ["wav_sample_provider.cc"],
    hdrs = ["wav_sample_provider.h"],
    deps = [
        ":audio_element_with_data",
        ":channel_label",
        ":demixing_module",
        ":wav_reader",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/common:macros",
        "//iamf/obu:types",
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
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_audio_to_tactile//:dsp",
    ],
)

cc_binary(
    name = "encoder_main",
    srcs = [
        "encoder_main.cc",
    ],
    data = [
        "//iamf/cli/testdata:input_wav_files",
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
        "@com_google_absl//absl/log:flags",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)
