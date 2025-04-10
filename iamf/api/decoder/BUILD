package(default_visibility = ["//iamf/api/decoder/tests:__pkg__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "iamf_decoder",
    srcs = ["iamf_decoder.cc"],
    hdrs = ["iamf_decoder.h"],
    deps = [
        "//iamf/api:iamf_tools_api_types",
        "//iamf/api/conversion:mix_presentation_conversion",
        "//iamf/api/conversion:profile_conversion",
        "//iamf/cli:obu_processor",
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/common:read_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

# keep-sorted end
