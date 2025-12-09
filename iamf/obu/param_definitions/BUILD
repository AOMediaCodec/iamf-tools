load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf:__subpackages__"])

cc_library(
    name = "polar_param_definition",
    srcs = ["polar_param_definition.cc"],
    hdrs = ["polar_param_definition.h"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:param_definitions",
        "//iamf/obu:polar_parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)
