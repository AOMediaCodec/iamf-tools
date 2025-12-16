load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "cart16_param_definition_test",
    srcs = ["cart16_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:cart16_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cart8_param_definition_test",
    srcs = ["cart8_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:cart8_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "demixing_param_definition_test",
    size = "small",
    srcs = ["demixing_param_definition_test.cc"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:demixing_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dual_cart16_param_definition_test",
    srcs = ["dual_cart16_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:dual_cart16_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dual_cart8_param_definition_test",
    srcs = ["dual_cart8_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:dual_cart8_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dual_polar_param_definition_test",
    srcs = ["dual_polar_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:dual_polar_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "extended_param_definition_test",
    srcs = ["extended_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:extended_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_gain_param_definition_test",
    srcs = ["mix_gain_param_definition_test.cc"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "param_definition_base_test",
    srcs = ["param_definition_base_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "polar_param_definition_test",
    size = "small",
    srcs = ["polar_param_definition_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/param_definitions:param_definition_base",
        "//iamf/obu/param_definitions:polar_param_definition",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "recon_gain_param_definition_test",
    srcs = ["recon_gain_param_definition_test.cc"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:param_definition_base",
        "//iamf/obu/param_definitions:recon_gain_param_definition",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
