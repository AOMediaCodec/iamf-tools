load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "iamf_decoder_factory",
    srcs = ["iamf_decoder_factory.cc"],
    hdrs = ["iamf_decoder_factory.h"],
    visibility = [
        "//iamf/include/iamf_tools/tests:__pkg__",
    ],
    deps = [
        ":iamf_decoder_interface",
        ":iamf_tools_api_types",
        "//iamf/api/decoder:iamf_decoder",
    ],
)

cc_library(
    name = "iamf_decoder_interface",
    hdrs = ["iamf_decoder_interface.h"],
    visibility = [
        "//iamf/api/decoder:__pkg__",
    ],
    deps = [
        "//iamf/include/iamf_tools:iamf_tools_api_types",
    ],
)

cc_library(
    name = "iamf_encoder_factory",
    srcs = ["iamf_encoder_factory.cc"],
    hdrs = ["iamf_encoder_factory.h"],
    visibility = [
        "//iamf/api:__subpackages__",
        "//iamf/include/iamf_tools/tests:__pkg__",
    ],
    deps = [
        "//iamf/cli:iamf_components",
        "//iamf/cli:iamf_encoder",
        "//iamf/cli:obu_sequencer_base",
        "//iamf/cli:obu_sequencer_iamf",
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion:proto_utils",
        "//iamf/include/iamf_tools:iamf_encoder_interface",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
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
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "iamf_tools_api_types",
    srcs = ["iamf_tools_api_types.cc"],
    hdrs = ["iamf_tools_api_types.h"],
    visibility = [
        "//iamf/api:__subpackages__",
        "//iamf/cli/internal/decoder:__subpackages__",
        "//iamf/include/iamf_tools/tests:__pkg__",
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
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/types:span",
    ],
)

# keep-sorted end
