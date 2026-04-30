load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//iamf/cli:__subpackages__"])

cc_library(
    name = "precomputed_compressed_gains",
    srcs = ["precomputed_compressed_gains.cc"],
    hdrs = ["precomputed_compressed_gains.h"],
    deps = ["@abseil-cpp//absl/types:span"],
)

cc_library(
    name = "precomputed_compressed_gains_decoder",
    srcs = ["precomputed_compressed_gains_decoder.cc"],
    hdrs = ["precomputed_compressed_gains_decoder.h"],
    deps = [
        ":compressed_gains_codebook",
        ":precomputed_compressed_gains",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "compressed_gains_codebook",
    hdrs = ["compressed_gains_codebook.h"],
    deps = ["@abseil-cpp//absl/base:no_destructor"],
)

cc_library(
    name = "precomputed_gains",
    srcs = ["precomputed_gains.cc"],
    hdrs = ["precomputed_gains.h"],
    deps = ["@abseil-cpp//absl/container:flat_hash_map"],
)

cc_library(
    name = "get_gains",
    srcs = ["get_gains.cc"],
    hdrs = ["get_gains.h"],
    deps = [
        ":precomputed_compressed_gains",
        ":precomputed_compressed_gains_decoder",
        "//iamf/common/utils:map_utils",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "get_gains_test",
    srcs = ["get_gains_test.cc"],
    deps = [
        ":get_gains",
        ":precomputed_compressed_gains",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "precomputed_compressed_gains_decoder_test",
    srcs = ["precomputed_compressed_gains_decoder_test.cc"],
    deps = [
        ":precomputed_compressed_gains",
        ":precomputed_compressed_gains_decoder",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
    ],
)
