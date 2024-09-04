# Tests for the IAMF software.

cc_test(
    name = "bit_buffer_util_test",
    srcs = ["bit_buffer_util_test.cc"],
    deps = [
        "//iamf/common:bit_buffer_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_util_test",
    size = "small",
    srcs = ["obu_util_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:obu_util",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "read_bit_buffer_test",
    srcs = ["read_bit_buffer_test.cc"],
    deps = [
        "//iamf/common:bit_buffer_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/obu:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_utils",
    testonly = True,
    srcs = ["test_utils.cc"],
    hdrs = ["test_utils.h"],
    visibility = [
        "//iamf/common/tests:__subpackages__",
        "//iamf/obu/decoder_config/tests:__subpackages__",
        "//iamf/obu/tests:__subpackages__",
    ],
    deps = [
        "//iamf/common:write_bit_buffer",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "write_bit_buffer_test",
    size = "small",
    srcs = ["write_bit_buffer_test.cc"],
    deps = [
        ":test_utils",
        "//iamf/cli:leb_generator",
        "//iamf/common:bit_buffer_util",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
