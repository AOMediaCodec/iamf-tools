load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//validation:__subpackages__"])

cc_library(
    name = "opus_hoa",
    srcs = ["opus_hoa.cc"],
    hdrs = ["opus_hoa.h"],
    deps = [
        "//iamf/cli:descriptor_obu_parser",
        "//iamf/cli:descriptor_obus",
        "//iamf/common:read_bit_buffer",
        "//iamf/obu:ambisonics_config",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_binary(
    name = "opus_hoa_main",
    srcs = ["opus_hoa_main.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":opus_hoa",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:flags",
    ],
)
