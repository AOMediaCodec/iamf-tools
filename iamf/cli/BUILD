load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

# Example commandline interface tools for the IAMF software.
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    # TODO(b/400697000): Break up visibility for the api directory.
    "//iamf/api:__subpackages__",
    "//iamf/include/iamf_tools:__subpackages__",
    "//iamf/cli:__subpackages__",
])

exports_files(
    srcs = [
        "encoder_main.cc",
        "encoder_main_lib.cc",
        "encoder_main_lib.h",
        "iamf_components.h",
    ],
)

# keep-sorted start block=yes prefix_order=cc_library,cc_binary newline_separated=yes
cc_library(
    name = "audio_element_with_data",
    hdrs = ["audio_element_with_data.h"],
    deps = [
        ":channel_label",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
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
        "//iamf/cli/codec:flac_decoder",
        "//iamf/cli/codec:lpcm_decoder",
        "//iamf/cli/codec:opus_decoder",
        "//iamf/common/utils:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "audio_frame_with_data",
    hdrs = ["audio_frame_with_data.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/obu:audio_frame",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "channel_label",
    srcs = ["channel_label.cc"],
    hdrs = ["channel_label.h"],
    deps = [
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "cli_util",
    srcs = ["cli_util.cc"],
    hdrs = ["cli_util.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "demixing_module",
    srcs = ["demixing_module.cc"],
    hdrs = ["demixing_module.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":channel_label",
        ":cli_util",
        ":substream_frames",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/container:node_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "descriptor_obu_parser",
    srcs = ["descriptor_obu_parser.cc"],
    hdrs = ["descriptor_obu_parser.h"],
    deps = [
        ":audio_element_with_data",
        ":obu_with_data_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "encoder_main_lib",
    srcs = ["encoder_main_lib.cc"],
    hdrs = ["encoder_main_lib.h"],
    deps = [
        ":audio_element_with_data",
        ":demixing_module",
        ":iamf_components",
        ":iamf_encoder",
        ":parameter_block_partitioner",
        ":rendering_mix_presentation_finalizer",
        ":sample_processor_base",
        ":wav_sample_provider",
        ":wav_writer",
        "//iamf/cli/proto:encoder_control_metadata_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion:channel_label_utils",
        "//iamf/cli/proto_conversion:output_audio_format_utils",
        "//iamf/common/utils:macros",
        "//iamf/include/iamf_tools:iamf_tools_encoder_api_types",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "iamf_components",
    srcs = ["iamf_components.cc"],
    hdrs = ["iamf_components.h"],
    deps = [
        ":loudness_calculator_factory_base",
        ":obu_sequencer_base",
        ":obu_sequencer_iamf",
        ":renderer_factory",
        "//iamf/cli/itu_1770_4:loudness_calculator_factory_itu_1770_4",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion:proto_utils",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
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
        ":cli_util",
        ":demixing_module",
        ":global_timing_module",
        ":loudness_calculator_factory_base",
        ":obu_sequencer_base",
        ":obu_sequencer_streaming_iamf",
        ":parameter_block_with_data",
        ":parameters_manager",
        ":renderer_factory",
        ":rendering_mix_presentation_finalizer",
        ":temporal_unit_view",
        "//iamf/cli/proto:encoder_control_metadata_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion:channel_label_utils",
        "//iamf/cli/proto_conversion:downmixing_reconstruction_util",
        "//iamf/cli/proto_conversion:proto_utils",
        "//iamf/cli/proto_conversion/proto_to_obu:arbitrary_obu_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:audio_element_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:audio_frame_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:codec_config_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:ia_sequence_header_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:mix_presentation_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:parameter_block_generator",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/include/iamf_tools:iamf_encoder_interface",
        "//iamf/include/iamf_tools:iamf_tools_encoder_api_types",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "loudness_calculator_base",
    srcs = ["loudness_calculator_base.cc"],
    hdrs = ["loudness_calculator_base.h"],
    deps = [
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "loudness_calculator_factory_base",
    srcs = ["loudness_calculator_factory_base.cc"],
    hdrs = ["loudness_calculator_factory_base.h"],
    deps = [
        ":loudness_calculator_base",
        "//iamf/obu:mix_presentation",
    ],
)

cc_library(
    name = "obu_processor",
    srcs = ["obu_processor.cc"],
    hdrs = ["obu_processor.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_decoder",
        ":audio_frame_with_data",
        ":cli_util",
        ":demixing_module",
        ":global_timing_module",
        ":obu_processor_utils",
        ":obu_with_data_generator",
        ":parameter_block_with_data",
        ":parameters_manager",
        ":profile_filter",
        ":renderer_factory",
        ":rendering_mix_presentation_finalizer",
        ":sample_processor_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:temporal_delimiter",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "obu_processor_utils",
    srcs = ["obu_processor_utils.cc"],
    hdrs = ["obu_processor_utils.h"],
    deps = [
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "obu_sequencer_base",
    srcs = ["obu_sequencer_base.cc"],
    hdrs = ["obu_sequencer_base.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":cli_util",
        ":parameter_block_with_data",
        ":profile_filter",
        ":temporal_unit_view",
        "//iamf/common:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_block",
        "//iamf/obu:temporal_delimiter",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "obu_sequencer_iamf",
    srcs = ["obu_sequencer_iamf.cc"],
    hdrs = ["obu_sequencer_iamf.h"],
    deps = [
        ":obu_sequencer_base",
        "//iamf/common:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "obu_sequencer_streaming_iamf",
    srcs = ["obu_sequencer_streaming_iamf.cc"],
    hdrs = ["obu_sequencer_streaming_iamf.h"],
    deps = [
        ":obu_sequencer_base",
        "//iamf/common:leb_generator",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "obu_with_data_generator",
    srcs = ["obu_with_data_generator.cc"],
    hdrs = ["obu_with_data_generator.h"],
    deps = [
        ":audio_element_with_data",
        ":audio_frame_with_data",
        ":channel_label",
        ":cli_util",
        ":global_timing_module",
        ":parameter_block_with_data",
        ":parameters_manager",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
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
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:obu_util",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "parameter_block_with_data",
    hdrs = ["parameter_block_with_data.h"],
    deps = [
        "//iamf/obu:parameter_block",
        "//iamf/obu:types",
    ],
)

cc_library(
    name = "parameters_manager",
    srcs = ["parameters_manager.cc"],
    hdrs = ["parameters_manager.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        ":parameter_block_with_data",
        "//iamf/common/utils:macros",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:absl_vlog_is_on",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "profile_filter",
    srcs = ["profile_filter.cc"],
    hdrs = ["profile_filter.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "recon_gain_generator",
    srcs = ["recon_gain_generator.cc"],
    hdrs = ["recon_gain_generator.h"],
    deps = [
        ":channel_label",
        ":demixing_module",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "renderer_factory",
    srcs = ["renderer_factory.cc"],
    hdrs = ["renderer_factory.h"],
    deps = [
        ":audio_element_with_data",
        "//iamf/cli/renderer:audio_element_renderer_ambisonics_to_channel",
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/cli/renderer:audio_element_renderer_channel_to_channel",
        "//iamf/cli/renderer:audio_element_renderer_passthrough",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
    ],
)

cc_library(
    name = "rendering_mix_presentation_finalizer",
    srcs = ["rendering_mix_presentation_finalizer.cc"],
    hdrs = ["rendering_mix_presentation_finalizer.h"],
    deps = [
        ":audio_element_with_data",
        ":cli_util",
        ":demixing_module",
        ":loudness_calculator_base",
        ":loudness_calculator_factory_base",
        ":parameter_block_with_data",
        ":renderer_factory",
        ":sample_processor_base",
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "sample_processor_base",
    srcs = ["sample_processor_base.cc"],
    hdrs = ["sample_processor_base.h"],
    deps = [
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "substream_frames",
    hdrs = ["substream_frames.h"],
    deps = [
        "@abseil-cpp//absl/log:absl_check",
    ],
)

cc_library(
    name = "temporal_unit_view",
    srcs = ["temporal_unit_view.cc"],
    hdrs = ["temporal_unit_view.h"],
    deps = [
        ":audio_frame_with_data",
        ":parameter_block_with_data",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:types",
        "@abseil-cpp//absl/algorithm:container",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "wav_reader",
    srcs = ["wav_reader.cc"],
    hdrs = ["wav_reader.h"],
    deps = [
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
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
        "//iamf/cli/proto_conversion:channel_label_utils",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:codec_config",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "wav_writer",
    srcs = ["wav_writer.cc"],
    hdrs = ["wav_writer.h"],
    deps = [
        ":sample_processor_base",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@com_google_audio_to_tactile//:dsp",
    ],
)

cc_binary(
    name = "decoder_main",
    srcs = ["decoder_main.cc"],
    deps = [
        ":wav_writer",
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/api/internal_utils",
        "//iamf/common/utils:map_utils",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

# keep-sorted end
