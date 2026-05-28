load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//validation:__subpackages__"])

cc_library(
    name = "iamf_loudness_comparator",
    srcs = ["iamf_loudness_comparator.cc"],
    hdrs = ["iamf_loudness_comparator.h"],
    deps = [
        "//iamf/cli:descriptor_obu_parser",
        "//iamf/cli:descriptor_obus",
        "//iamf/common:q_format_or_floating_point",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_binary(
    name = "iamf_loudness_comparator_main",
    srcs = ["iamf_loudness_comparator_main.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":iamf_loudness_comparator",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:flags",
    ],
)
