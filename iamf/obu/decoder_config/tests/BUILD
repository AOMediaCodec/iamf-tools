# Tests for the IAMF software.

load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "aac_decoder_config_test",
    srcs = ["aac_decoder_config_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "flac_decoder_config_test",
    srcs = ["flac_decoder_config_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lpcm_decoder_config_test",
    srcs = ["lpcm_decoder_config_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_decoder_config_test",
    srcs = ["opus_decoder_config_test.cc"],
    deps = [
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils/tests:test_utils",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
