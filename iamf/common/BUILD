load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load(":build_info.bzl", "generate_build_info")

package(default_visibility = [
    "//iamf:__subpackages__",
    "//validation:__subpackages__",
])

# keep-sorted start block=yes prefix_order=bzl_library,generate_build_info,cc_library newline_separated=yes
bzl_library(
    name = "build_info_bzl",
    srcs = ["build_info.bzl"],
    visibility = ["//visibility:private"],
)

generate_build_info(
    name = "generated_build_info",
    out = "generated_build_info.cc",
)

cc_library(
    name = "build_info",
    srcs = [
        "build_info.cc",
        ":generated_build_info",
    ],
    hdrs = ["build_info.h"],
    deps = [
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "leb_generator",
    srcs = ["leb_generator.cc"],
    hdrs = ["leb_generator.h"],
    deps = [
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "q_format_or_floating_point",
    srcs = ["q_format_or_floating_point.cc"],
    hdrs = ["q_format_or_floating_point.h"],
    deps = [
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "read_bit_buffer",
    srcs = ["read_bit_buffer.cc"],
    hdrs = ["read_bit_buffer.h"],
    deps = [
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "write_bit_buffer",
    srcs = ["write_bit_buffer.cc"],
    hdrs = ["write_bit_buffer.h"],
    deps = [
        ":leb_generator",
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

# keep-sorted end
