load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=filegroup,cc_library newline_separated=yes
filegroup(
    name = "iamf_decoder_factory_header",
    srcs = ["iamf_decoder_factory.h"],
    visibility = [
        "//iamf/api/decoder:__pkg__",
    ],
)

filegroup(
    name = "iamf_decoder_header",
    srcs = ["iamf_decoder.h"],
    visibility = [
        "//iamf/api/decoder:__pkg__",
    ],
)

filegroup(
    name = "iamf_encoder_factory_header",
    srcs = ["iamf_encoder_factory.h"],
    visibility = [
        "//iamf/api/encoder:__pkg__",
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
    name = "iamf_decoder_interface",
    hdrs = ["iamf_decoder_interface.h"],
    visibility = [
        "//iamf/api/decoder:__pkg__",
    ],
    deps = [
        "//iamf/api:iamf_tools_api_types",
    ],
)

cc_library(
    name = "iamf_encoder_interface",
    hdrs = ["iamf_encoder_interface.h"],
    visibility = [
        "//iamf/api:__subpackages__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/tests:__pkg__",
    ],
    deps = [
        ":iamf_tools_encoder_api_types",
        "@com_google_absl//absl/status",
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

# keep-sorted end
