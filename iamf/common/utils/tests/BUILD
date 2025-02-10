# Tests for the IAMF software.

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "bit_buffer_util_test",
    srcs = ["bit_buffer_util_test.cc"],
    deps = [
        "//iamf/common/utils:bit_buffer_util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "map_utils_test",
    srcs = ["map_utils_test.cc"],
    deps = [
        "//iamf/common/utils:map_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "numeric_utils_test",
    srcs = ["numeric_utils_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sample_processing_utils_test",
    srcs = ["sample_processing_utils_test.cc"],
    deps = [
        "//iamf/common/utils:sample_processing_utils",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "validation_utils_test",
    size = "small",
    srcs = ["validation_utils_test.cc"],
    deps = [
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
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

# keep-sorted end
