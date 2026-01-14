load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
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
