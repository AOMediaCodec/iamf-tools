# Conversion of proto to internal representations.
# cli/proto_to_obu and cli/obu_to_proto are subdirectories for unidirectional conversion, but utils
# and bidirectional conversion can stay in this directory.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf/cli/proto_conversion:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "channel_label_utils",
    srcs = ["channel_label_utils.cc"],
    hdrs = ["channel_label_utils.h"],
    visibility = [
        # TODO(b/400697000): Break up visibility for the api directory.
        "//iamf/api:__subpackages__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/internal/transmuxer:__pkg__",
        "//iamf/cli/proto_conversion/proto_to_obu:__pkg__",
        "//iamf/cli/proto_conversion/tests:__pkg__",
        "//iamf/cli/tests:__pkg__",
        "//iamf/cli/user_metadata_builder/tests:__pkg__",
    ],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/common/utils:map_utils",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "codec_config_utils",
    srcs = ["codec_config_utils.cc"],
    hdrs = ["codec_config_utils.h"],
    deps = [
        "//iamf/cli/codec:opus_encoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@libopus",
    ],
)

cc_library(
    name = "downmixing_reconstruction_util",
    srcs = ["downmixing_reconstruction_util.cc"],
    hdrs = ["downmixing_reconstruction_util.h"],
    visibility = [
        "//iamf/cli:__pkg__",
        "//iamf/cli/proto_conversion/tests:__pkg__",
        "//iamf/cli/tests:__pkg__",
    ],
    deps = [
        ":channel_label_utils",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "lookup_tables",
    hdrs = ["lookup_tables.h"],
    deps = [
        "//iamf/cli/proto:arbitrary_obu_cc_proto",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/obu:audio_element",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_data",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
    ],
)

cc_library(
    name = "output_audio_format_utils",
    srcs = ["output_audio_format_utils.cc"],
    hdrs = ["output_audio_format_utils.h"],
    visibility = [
        "//iamf/cli:__pkg__",
        "//iamf/cli/proto_conversion:__subpackages__",
    ],
    deps = [
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/cli:sample_processor_base",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:output_audio_format_cc_proto",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
    ],
)

cc_library(
    name = "proto_utils",
    srcs = ["proto_utils.cc"],
    hdrs = ["proto_utils.h"],
    visibility = [
        # TODO(b/400697000): Break up visibility for the api directory.
        "//iamf/api:__subpackages__",
        "//iamf/include/iamf_tools:__subpackages__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/proto_conversion:__subpackages__",
    ],
    deps = [
        ":lookup_tables",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/common:leb_generator",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

# keep-sorted end
