load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "loudness_calculator_factory_itu_1770_4_test",
    srcs = ["loudness_calculator_factory_itu_1770_4_test.cc"],
    deps = [
        "//iamf/cli/itu_1770_4:loudness_calculator_factory_itu_1770_4",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/obu:mix_presentation",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "loudness_calculator_itu_1770_4_test",
    srcs = ["loudness_calculator_itu_1770_4_test.cc"],
    deps = [
        "//iamf/cli/itu_1770_4:loudness_calculator_itu_1770_4",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "loudness_calculator_itu_1770_benchmark",
    srcs = ["loudness_calculator_itu_1770_benchmark.cc"],
    deps = [
        "//iamf/cli/itu_1770_4:loudness_calculator_itu_1770_4",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
        "@com_google_benchmark//:benchmark_main",
    ],
)

# keep-sorted end
