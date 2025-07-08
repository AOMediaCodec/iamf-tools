load("@rules_cc//cc:cc_library.bzl", "cc_library")

filegroup(
    name = "iamf_decoder_header",
    srcs = ["iamf_decoder.h"],
    visibility = [
        "//iamf/api/decoder:__pkg__",
    ],
)

filegroup(
    name = "iamf_tools_api_types_header",
    srcs = ["iamf_tools_api_types.h"],
    visibility = [
        "//iamf/api:__pkg__",
    ],
)

cc_library(
    name = "iamf_tools_encoder_api_types",
    hdrs = ["iamf_tools_encoder_api_types.h"],
    visibility = [
        "//iamf/api:__subpackages__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/tests:__pkg__",
    ],
    deps = [
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/types:span",
    ],
)
