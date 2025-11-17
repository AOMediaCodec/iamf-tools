load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    "//iamf/cli/adm_to_user_metadata/iamf:__subpackages__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "ia_sequence_header_obu_metadata_handler",
    srcs = [
        "ia_sequence_header_obu_metadata_handler.cc",
    ],
    hdrs = ["ia_sequence_header_obu_metadata_handler.h"],
    deps = ["//iamf/cli/proto:ia_sequence_header_cc_proto"],
)

cc_library(
    name = "iamf",
    srcs = [
        "iamf.cc",
    ],
    hdrs = ["iamf.h"],
    visibility = ["//iamf/cli/adm_to_user_metadata/iamf/tests:__pkg__"],
    deps = [
        ":mix_presentation_handler",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/user_metadata_builder:audio_element_metadata_builder",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "mix_presentation_handler",
    srcs = ["mix_presentation_handler.cc"],
    hdrs = ["mix_presentation_handler.h"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:numeric_utils",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "test_vector_metadata_handler",
    srcs = ["test_vector_metadata_handler.cc"],
    hdrs = ["test_vector_metadata_handler.h"],
    deps = [
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "user_metadata_generator",
    srcs = ["user_metadata_generator.cc"],
    hdrs = [
        "user_metadata_generator.h",
    ],
    visibility = [
        "//iamf/cli/adm_to_user_metadata/app:__pkg__",
        "//iamf/cli/adm_to_user_metadata/iamf/tests:__pkg__",
    ],
    deps = [
        ":ia_sequence_header_obu_metadata_handler",
        ":iamf",
        ":mix_presentation_handler",
        ":test_vector_metadata_handler",
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/user_metadata_builder:audio_frame_metadata_builder",
        "//iamf/cli/user_metadata_builder:codec_config_obu_metadata_builder",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# keep-sorted end
