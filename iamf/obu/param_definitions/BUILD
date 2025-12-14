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

cc_library(
    name = "dual_polar_param_definition",
    srcs = ["dual_polar_param_definition.cc"],
    hdrs = ["dual_polar_param_definition.h"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:dual_polar_parameter_data",
        "//iamf/obu:param_definitions",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "cart8_param_definition",
    srcs = ["cart8_param_definition.cc"],
    hdrs = ["cart8_param_definition.h"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:cart8_parameter_data",
        "//iamf/obu:param_definitions",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "cart16_param_definition",
    srcs = ["cart16_param_definition.cc"],
    hdrs = ["cart16_param_definition.h"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:cart8_parameter_data",
        "//iamf/obu:param_definitions",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "dual_cart8_param_definition",
    srcs = ["dual_cart8_param_definition.cc"],
    hdrs = ["dual_cart8_param_definition.h"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:dual_cart8_parameter_data",
        "//iamf/obu:param_definitions",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)
