# Tests for the IAMF software.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "obu_test_base",
    testonly = True,
    srcs = ["obu_test_base.cc"],
    hdrs = ["obu_test_base.h"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:obu_header",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "obu_test_utils",
    testonly = True,
    hdrs = ["obu_test_utils.h"],
    visibility = ["//iamf:__subpackages__"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:obu_base",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "arbitrary_obu_test",
    srcs = ["arbitrary_obu_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_test",
    size = "small",
    srcs = ["audio_element_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:demixing_param_definition",
        "//iamf/obu/param_definitions:extended_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_frame_test",
    size = "small",
    srcs = ["audio_frame_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:audio_frame",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "codec_config_test",
    size = "small",
    srcs = ["codec_config_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "element_gain_offset_config_test",
    srcs = ["element_gain_offset_config_test.cc"],
    deps = [
        "//iamf/common:q_format_or_floating_point",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:element_gain_offset_config",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ia_sequence_header_test",
    size = "small",
    srcs = ["ia_sequence_header_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "metadata_obu_test",
    size = "small",
    srcs = ["metadata_obu_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:metadata_obu",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_test",
    size = "small",
    srcs = ["mix_presentation_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:q_format_or_floating_point",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:element_gain_offset_config",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:rendering_config",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_base_test",
    srcs = ["obu_base_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:obu_base",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_header_test",
    size = "small",
    srcs = ["obu_header_test.cc"],
    deps = [
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameter_block_test",
    size = "small",
    srcs = ["parameter_block_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:demixing_param_definition",
        "//iamf/obu/param_definitions:extended_param_definition",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "//iamf/obu/param_definitions:recon_gain_param_definition",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameter_data_test",
    size = "small",
    srcs = [
        "demixing_info_parameter_data_test.cc",
        "extension_parameter_data_test.cc",
        "mix_gain_parameter_data_test.cc",
        "recon_gain_info_parameter_data_test.cc",
    ],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "rendering_config_test",
    srcs = ["rendering_config_test.cc"],
    deps = [
        "//iamf/common:q_format_or_floating_point",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu:element_gain_offset_config",
        "//iamf/obu:rendering_config",
        "//iamf/obu/param_definitions:cart16_param_definition",
        "//iamf/obu/param_definitions:cart8_param_definition",
        "//iamf/obu/param_definitions:dual_cart16_param_definition",
        "//iamf/obu/param_definitions:dual_cart8_param_definition",
        "//iamf/obu/param_definitions:dual_polar_param_definition",
        "//iamf/obu/param_definitions:param_definition_base",
        "//iamf/obu/param_definitions:polar_param_definition",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "temporal_delimiter_test",
    size = "small",
    srcs = ["temporal_delimiter_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:obu_header",
        "//iamf/obu:temporal_delimiter",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
