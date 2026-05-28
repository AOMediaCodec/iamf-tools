load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "iamf_loudness_comparator_test",
    srcs = ["iamf_loudness_comparator_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:q_format_or_floating_point",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "//iamf/obu/tests:obu_test_utils",
        "//validation/iamf_loudness_comparator",
        "@googletest//:gtest_main",
    ],
)
