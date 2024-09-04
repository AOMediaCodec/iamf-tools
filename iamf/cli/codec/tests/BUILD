cc_library(
    name = "encoder_test_base",
    testonly = True,
    srcs = ["encoder_test_base.cc"],
    hdrs = ["encoder_test_base.h"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/codec:encoder_base",
        "//iamf/obu:audio_frame",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "aac_encoder_test",
    srcs = ["aac_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf/cli/codec:aac_encoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "decoder_base_test",
    srcs = ["decoder_base_test.cc"],
    deps = [
        "//iamf/cli/codec:decoder_base",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoder_base_test",
    srcs = ["encoder_base_test.cc"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/codec:encoder_base",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "flac_encoder_test",
    srcs = ["flac_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf/cli/codec:flac_encoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lpcm_decoder_test",
    size = "small",
    srcs = ["lpcm_decoder_test.cc"],
    deps = [
        "//iamf/cli/codec:lpcm_decoder",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lpcm_encoder_test",
    size = "small",
    srcs = ["lpcm_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf/cli/codec:lpcm_encoder",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_encoder_test",
    srcs = ["opus_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf/cli/codec:opus_encoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
