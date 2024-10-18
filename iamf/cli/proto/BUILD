# Protos for IAMF data.

# [internal] load cc_proto_library.bzl

package(default_visibility = [
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
])

proto_library(
    name = "arbitrary_obu_proto",
    srcs = ["arbitrary_obu.proto"],
    deps = [":obu_header_proto"],
)

proto_library(
    name = "audio_element_proto",
    srcs = ["audio_element.proto"],
    deps = [
        "param_definitions_proto",
        ":obu_header_proto",
    ],
)

proto_library(
    name = "audio_frame_proto",
    srcs = ["audio_frame.proto"],
)

proto_library(
    name = "codec_config_proto",
    srcs = ["codec_config.proto"],
    deps = [":obu_header_proto"],
)

proto_library(
    name = "ia_sequence_header_proto",
    srcs = ["ia_sequence_header.proto"],
    deps = [":obu_header_proto"],
)

proto_library(
    name = "mix_presentation_proto",
    srcs = ["mix_presentation.proto"],
    deps = [
        ":obu_header_proto",
        ":param_definitions_proto",
    ],
)

proto_library(
    name = "obu_header_proto",
    srcs = ["obu_header.proto"],
)

proto_library(
    name = "param_definitions_proto",
    srcs = ["param_definitions.proto"],
    deps = [":parameter_data_proto"],
)

proto_library(
    name = "parameter_block_proto",
    srcs = ["parameter_block.proto"],
    deps = [
        ":obu_header_proto",
        ":parameter_data_proto",
    ],
)

proto_library(
    name = "parameter_data_proto",
    srcs = ["parameter_data.proto"],
)

proto_library(
    name = "temporal_delimiter_proto",
    srcs = ["temporal_delimiter.proto"],
)

proto_library(
    name = "test_vector_metadata_proto",
    srcs = ["test_vector_metadata.proto"],
)

proto_library(
    name = "user_metadata_proto",
    srcs = ["user_metadata.proto"],
    deps = [
        ":arbitrary_obu_proto",
        ":audio_element_proto",
        ":audio_frame_proto",
        ":codec_config_proto",
        ":ia_sequence_header_proto",
        ":mix_presentation_proto",
        ":parameter_block_proto",
        ":temporal_delimiter_proto",
        ":test_vector_metadata_proto",
    ],
)

cc_proto_library(
    name = "arbitrary_obu_cc_proto",
    deps = [":arbitrary_obu_proto"],
)

cc_proto_library(
    name = "audio_element_cc_proto",
    deps = [":audio_element_proto"],
)

cc_proto_library(
    name = "audio_frame_cc_proto",
    deps = [":audio_frame_proto"],
)

cc_proto_library(
    name = "codec_config_cc_proto",
    deps = [":codec_config_proto"],
)

cc_proto_library(
    name = "ia_sequence_header_cc_proto",
    deps = [":ia_sequence_header_proto"],
)

cc_proto_library(
    name = "mix_presentation_cc_proto",
    deps = [":mix_presentation_proto"],
)

cc_proto_library(
    name = "obu_header_cc_proto",
    deps = [":obu_header_proto"],
)

cc_proto_library(
    name = "param_definitions_cc_proto",
    deps = [":param_definitions_proto"],
)

cc_proto_library(
    name = "parameter_block_cc_proto",
    deps = [":parameter_block_proto"],
)

cc_proto_library(
    name = "parameter_data_cc_proto",
    deps = [":parameter_data_proto"],
)

cc_proto_library(
    name = "temporal_delimiter_cc_proto",
    deps = [":temporal_delimiter_proto"],
)

cc_proto_library(
    name = "test_vector_metadata_cc_proto",
    deps = [":test_vector_metadata_proto"],
)

cc_proto_library(
    name = "user_metadata_cc_proto",
    deps = [":user_metadata_proto"],
)
