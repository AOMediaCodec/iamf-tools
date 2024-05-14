# Tests for the IAMF software.

package(default_visibility = ["//iamf/cli/tests:__subpackages__"])

cc_library(
    name = "obu_test_base",
    testonly = True,
    srcs = ["obu_test_base.cc"],
    hdrs = ["obu_test_base.h"],
    deps = [
        "//iamf/cli:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:obu_header",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "arbitrary_obu_test",
    srcs = ["arbitrary_obu_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:arbitrary_obu",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_test",
    size = "small",
    srcs = ["audio_element_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:audio_element",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_frame_test",
    size = "small",
    srcs = ["audio_frame_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:audio_frame",
        "//iamf/obu:leb128",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "codec_config_test",
    size = "small",
    srcs = ["codec_config_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:codec_config",
        "//iamf/obu:leb128",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "demixing_info_param_data_test",
    srcs = ["demixing_info_param_data_test.cc"],
    deps = [
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:demixing_info_param_data",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ia_sequence_header_test",
    size = "small",
    srcs = ["ia_sequence_header_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_test",
    size = "small",
    srcs = ["mix_presentation_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:bit_buffer_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:leb128",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_base_test",
    srcs = ["obu_base_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:obu_base",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_header_test",
    size = "small",
    srcs = ["obu_header_test.cc"],
    deps = [
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "param_definitions_test",
    size = "small",
    srcs = ["param_definitions_test.cc"],
    deps = [
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/tests:test_utils",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "//iamf/obu:param_definitions",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameter_block_test",
    size = "small",
    srcs = ["parameter_block_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "temporal_delimiter_test",
    size = "small",
    srcs = ["temporal_delimiter_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf/cli:leb_generator",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:obu_header",
        "//iamf/obu:temporal_delimiter",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
