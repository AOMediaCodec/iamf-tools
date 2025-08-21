# Tests for the IAMF software.

load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "leb_generator_test",
    srcs = ["leb_generator_test.cc"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "read_bit_buffer_fuzz_test",
    size = "small",
    srcs = ["read_bit_buffer_fuzz_test.cc"],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
        "@com_google_fuzztest//fuzztest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "read_bit_buffer_test",
    srcs = ["read_bit_buffer_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:read_bit_buffer",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "write_bit_buffer_fuzz_test",
    size = "small",
    srcs = ["write_bit_buffer_fuzz_test.cc"],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "//iamf/common:write_bit_buffer",
        "@abseil-cpp//absl/types:span",
        "@com_google_fuzztest//fuzztest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "write_bit_buffer_test",
    size = "small",
    srcs = ["write_bit_buffer_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "write_read_fuzz_test",
    size = "small",
    srcs = ["write_read_fuzz_test.cc"],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_fuzztest//fuzztest",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
