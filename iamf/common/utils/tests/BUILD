# Tests for the IAMF software.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "map_utils_test",
    srcs = ["map_utils_test.cc"],
    deps = [
        "//iamf/common/utils:map_utils",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/functional:function_ref",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "numeric_utils_test",
    srcs = ["numeric_utils_test.cc"],
    deps = [
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "sample_processing_utils_test",
    srcs = ["sample_processing_utils_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/common/utils:sample_processing_utils",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "validation_utils_test",
    size = "small",
    srcs = ["validation_utils_test.cc"],
    deps = [
        "//iamf/common/utils:validation_utils",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest_main",
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
        "//iamf/obu/param_definitions/tests:__subpackages__",
        "//iamf/obu/tests:__subpackages__",
    ],
    deps = [
        "//iamf/common:write_bit_buffer",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest",
    ],
)

# keep-sorted end
