load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_library,cc_test newline_separated=yes
cc_library(
    name = "internal_utils",
    srcs = ["internal_utils.cc"],
    hdrs = ["internal_utils.h"],
    visibility = [
        "//iamf/cli:__subpackages__",
    ],
    deps = [
        "//iamf/cli:wav_writer",
        "//iamf/include/iamf_tools:iamf_decoder_interface",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "internal_utils_test",
    srcs = ["internal_utils_test.cc"],
    deps = [
        ":internal_utils",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:descriptor_obus",
        "//iamf/cli:wav_writer",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/include/iamf_tools:iamf_decoder_factory",
        "//iamf/include/iamf_tools:iamf_decoder_interface",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:audio_frame",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
    ],
)

# keep-sorted end
