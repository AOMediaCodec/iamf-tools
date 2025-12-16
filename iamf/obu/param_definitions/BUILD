load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "cart16_param_definition",
    srcs = ["cart16_param_definition.cc"],
    hdrs = ["cart16_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:cart16_parameter_data",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "cart8_param_definition",
    srcs = ["cart8_param_definition.cc"],
    hdrs = ["cart8_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:cart8_parameter_data",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "demixing_param_definition",
    srcs = ["demixing_param_definition.cc"],
    hdrs = ["demixing_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "dual_cart16_param_definition",
    srcs = ["dual_cart16_param_definition.cc"],
    hdrs = ["dual_cart16_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:dual_cart16_parameter_data",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "dual_cart8_param_definition",
    srcs = ["dual_cart8_param_definition.cc"],
    hdrs = ["dual_cart8_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:dual_cart8_parameter_data",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "dual_polar_param_definition",
    srcs = ["dual_polar_param_definition.cc"],
    hdrs = ["dual_polar_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:dual_polar_parameter_data",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "extended_param_definition",
    srcs = ["extended_param_definition.cc"],
    hdrs = ["extended_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "mix_gain_param_definition",
    srcs = ["mix_gain_param_definition.cc"],
    hdrs = ["mix_gain_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "param_definition_base",
    srcs = [
        "param_definition_base.cc",
    ],
    hdrs = [
        "param_definition_base.h",
    ],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "param_definition_variant",
    hdrs = ["param_definition_variant.h"],
    deps = [
        ":demixing_param_definition",
        ":extended_param_definition",
        ":mix_gain_param_definition",
        ":recon_gain_param_definition",
    ],
)

cc_library(
    name = "polar_param_definition",
    srcs = ["polar_param_definition.cc"],
    hdrs = ["polar_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:parameter_data",
        "//iamf/obu:polar_parameter_data",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
    ],
)

cc_library(
    name = "recon_gain_param_definition",
    srcs = ["recon_gain_param_definition.cc"],
    hdrs = ["recon_gain_param_definition.h"],
    deps = [
        ":param_definition_base",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

# keep-sorted end
