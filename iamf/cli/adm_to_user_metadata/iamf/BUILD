package(default_visibility = [
    "//iamf/cli/adm_to_user_metadata/iamf:__subpackages__",
])

cc_library(
    name = "audio_element_handler",
    srcs = ["audio_element_handler.cc"],
    hdrs = ["audio_element_handler.h"],
    deps = [
        ":iamf_input_layout",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_frame_handler",
    srcs = ["audio_frame_handler.cc"],
    hdrs = [
        "audio_frame_handler.h",
    ],
    deps = [
        ":iamf_input_layout",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "codec_config_obu_metadata_handler",
    srcs = ["codec_config_obu_metadata_handler.cc"],
    hdrs = [
        "codec_config_obu_metadata_handler.h",
    ],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/proto:codec_config_cc_proto",
    ],
)

cc_library(
    name = "ia_sequence_header_obu_metadata_handler",
    srcs = [
        "ia_sequence_header_obu_metadata_handler.cc",
    ],
    hdrs = ["ia_sequence_header_obu_metadata_handler.h"],
    deps = [
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/obu:ia_sequence_header",
    ],
)

cc_library(
    name = "iamf",
    srcs = [
        "iamf.cc",
    ],
    hdrs = ["iamf.h"],
    visibility = ["//iamf/cli/adm_to_user_metadata/iamf/tests:__pkg__"],
    deps = [
        ":audio_element_handler",
        ":audio_frame_handler",
        ":iamf_input_layout",
        ":mix_presentation_handler",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "iamf_input_layout",
    srcs = ["iamf_input_layout.cc"],
    hdrs = ["iamf_input_layout.h"],
    deps = [
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mix_presentation_handler",
    srcs = ["mix_presentation_handler.cc"],
    hdrs = ["mix_presentation_handler.h"],
    deps = [
        ":iamf_input_layout",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common:obu_util",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "test_vector_metadata_handler",
    srcs = ["test_vector_metadata_handler.cc"],
    hdrs = ["test_vector_metadata_handler.h"],
    deps = [
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@com_google_absl//absl/strings:string_view",
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
        ":codec_config_obu_metadata_handler",
        ":ia_sequence_header_obu_metadata_handler",
        ":iamf",
        ":mix_presentation_handler",
        ":test_vector_metadata_handler",
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
