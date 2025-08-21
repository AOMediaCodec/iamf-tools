load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    # TODO(b/400697000): Break up visibility for the api directory.
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
    "//iamf/include/iamf_tools/tests:__pkg__",
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
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
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
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
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
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# keep-sorted end
