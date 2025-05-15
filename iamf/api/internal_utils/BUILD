load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_library,cc_test newline_separated=yes
cc_library(
    name = "internal_utils",
    srcs = ["internal_utils.cc"],
    hdrs = ["internal_utils.h"],
    visibility = ["//iamf/cli/internal/decoder:__pkg__"],
    deps = [
        "//iamf/api:iamf_tools_api_types",
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli:wav_writer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "internal_utils_test",
    srcs = ["internal_utils_test.cc"],
    deps = [
        ":internal_utils",
        "//iamf/api:iamf_tools_api_types",
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:wav_writer",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
