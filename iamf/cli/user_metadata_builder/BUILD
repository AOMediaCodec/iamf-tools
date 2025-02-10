package(default_visibility = [
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "audio_element_metadata_builder",
    srcs = ["audio_element_metadata_builder.cc"],
    hdrs = ["audio_element_metadata_builder.h"],
    deps = [
        ":iamf_input_layout",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common/utils:map_utils",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_frame_metadata_builder",
    srcs = ["audio_frame_metadata_builder.cc"],
    hdrs = [
        "audio_frame_metadata_builder.h",
    ],
    deps = [
        ":iamf_input_layout",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common/utils:map_utils",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "codec_config_obu_metadata_builder",
    srcs = ["codec_config_obu_metadata_builder.cc"],
    hdrs = ["codec_config_obu_metadata_builder.h"],
    deps = [
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/obu:types",
    ],
)

cc_library(
    name = "iamf_input_layout",
    srcs = ["iamf_input_layout.cc"],
    hdrs = ["iamf_input_layout.h"],
    deps = [
        "//iamf/common/utils:map_utils",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

# keep-sorted end
