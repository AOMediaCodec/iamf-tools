load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "polar_param_definition_test",
    size = "small",
    srcs = ["polar_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:param_definitions",
        "//iamf/obu:polar_parameter_data",
        "//iamf/obu/param_definitions:polar_param_definition",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
