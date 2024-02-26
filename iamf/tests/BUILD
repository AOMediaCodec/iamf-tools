# Tests for the IAMF software.

package(default_visibility = ["//iamf/cli/tests:__subpackages__"])

cc_library(
    name = "obu_test_base",
    testonly = True,
    srcs = ["obu_test_base.cc"],
    hdrs = ["obu_test_base.h"],
    deps = [
        ":test_utils",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "test_utils",
    testonly = True,
    srcs = ["test_utils.cc"],
    hdrs = ["test_utils.h"],
    deps = [
        "//iamf:write_bit_buffer",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "aac_decoder_config_test",
    srcs = ["aac_decoder_config_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:aac_decoder_config",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "arbitrary_obu_test",
    srcs = ["arbitrary_obu_test.cc"],
    deps = [
        ":obu_test_base",
        ":test_utils",
        "//iamf:arbitrary_obu",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
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
        "//iamf:audio_element",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
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
        "//iamf:audio_frame",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
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
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:lpcm_decoder_config",
        "//iamf:opus_decoder_config",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "demixing_info_param_data_test",
    srcs = ["demixing_info_param_data_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:demixing_info_param_data",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "flac_decoder_config_test",
    srcs = ["flac_decoder_config_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:flac_decoder_config",
        "//iamf:write_bit_buffer",
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
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lpcm_decoder_config_test",
    srcs = ["lpcm_decoder_config_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:lpcm_decoder_config",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_test",
    size = "small",
    srcs = ["mix_presentation_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf:ia",
        "//iamf:mix_presentation",
        "//iamf:param_definitions",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_base_test",
    srcs = ["obu_base_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:ia",
        "//iamf:obu_base",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_header_test",
    size = "small",
    srcs = ["obu_header_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_util_test",
    size = "small",
    srcs = ["obu_util_test.cc"],
    deps = [
        "//iamf:obu_util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_decoder_config_test",
    srcs = ["opus_decoder_config_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:opus_decoder_config",
        "//iamf:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "param_definitions_test",
    size = "small",
    srcs = ["param_definitions_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
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
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "temporal_delimiter_test",
    size = "small",
    srcs = ["temporal_delimiter_test.cc"],
    deps = [
        ":obu_test_base",
        "//iamf:ia",
        "//iamf:temporal_delimiter",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "write_bit_buffer_test",
    size = "small",
    srcs = ["write_bit_buffer_test.cc"],
    deps = [
        ":test_utils",
        "//iamf:ia",
        "//iamf:write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "read_bit_buffer_test",
    srcs = ["read_bit_buffer_test.cc"],
    deps = [
        "//iamf:read_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bit_buffer_util_test",
    srcs = ["bit_buffer_util_test.cc"],
    deps = [
        "//iamf:bit_buffer_util",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
