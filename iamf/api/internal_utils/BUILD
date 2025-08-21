load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_library,cc_test newline_separated=yes
cc_library(
    name = "internal_utils",
    srcs = ["internal_utils.cc"],
    hdrs = ["internal_utils.h"],
    visibility = [
        "//iamf/cli/internal/decoder:__pkg__",
    ],
    deps = [
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli:wav_writer",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "internal_utils_test",
    srcs = ["internal_utils_test.cc"],
    deps = [
        ":internal_utils",
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:wav_writer",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
