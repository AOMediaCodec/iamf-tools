load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "iamf_encoder_factory",
    srcs = ["iamf_encoder_factory.cc"],
    hdrs = [
        "//iamf/include/iamf_tools:iamf_encoder_factory_header",
    ],
    visibility = [
        "//iamf/api:__subpackages__",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

# keep-sorted end
