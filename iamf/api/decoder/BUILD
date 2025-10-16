load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "iamf_decoder",
    srcs = ["iamf_decoder.cc"],
    hdrs = [
        "iamf_decoder.h",
    ],
    visibility = [
        "//iamf/api/decoder/tests:__pkg__",
        "//iamf/api/internal_utils:__pkg__",
        "//iamf/cli:__subpackages__",
        "//iamf/include/iamf_tools:__pkg__",
    ],
    deps = [
        "//iamf/api/conversion:channel_reorderer",
        "//iamf/api/conversion:mix_presentation_conversion",
        "//iamf/api/conversion:profile_conversion",
        "//iamf/cli:obu_processor",
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/common:read_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/include/iamf_tools:iamf_decoder_interface",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

# keep-sorted end
