load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "get_test_path",
    testonly = True,
    srcs = ["get_test_path.cc"],
    hdrs = ["get_test_path.h"],
    visibility = [
        "//iamf:__subpackages__",
    ],
    deps = ["@abseil-cpp//absl/strings:string_view"],
)

cc_test(
    name = "get_test_path_test",
    srcs = ["get_test_path_test.cc"],
    data = [
        "//iamf/cli/testdata:input_wav_files",
    ],
    deps = [
        ":get_test_path",
        "@googletest//:gtest_main",
    ],
)
