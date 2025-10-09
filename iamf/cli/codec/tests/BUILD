load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_library,cc_test newline_separated=yes
cc_library(
    name = "encoder_test_base",
    testonly = True,
    srcs = ["encoder_test_base.cc"],
    hdrs = ["encoder_test_base.h"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/codec:encoder_base",
        "//iamf/obu:audio_frame",
        "//iamf/obu:types",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "aac_decoder_test",
    srcs = ["aac_decoder_test.cc"],
    deps = [
        "//iamf/cli/codec:aac_decoder",
        "//iamf/cli/codec:decoder_base",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
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
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "decoder_base_test",
    srcs = ["decoder_base_test.cc"],
    deps = [
        "//iamf/cli/codec:decoder_base",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoder_base_test",
    srcs = ["encoder_base_test.cc"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/codec:encoder_base",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "flac_decoder_stream_callbacks_test",
    srcs = ["flac_decoder_stream_callbacks_test.cc"],
    deps = [
        "//iamf/cli/codec:flac_decoder_stream_callbacks",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:types",
        "@com_google_googletest//:gtest_main",
        "@flac//:src",
    ],
)

cc_test(
    name = "flac_decoder_test",
    srcs = ["flac_decoder_test.cc"],
    deps = [
        "//iamf/cli/codec:decoder_base",
        "//iamf/cli/codec:flac_decoder",
        "//iamf/cli/tests:cli_test_utils",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
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
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "lpcm_decoder_test",
    size = "small",
    srcs = ["lpcm_decoder_test.cc"],
    deps = [
        "//iamf/cli/codec:decoder_base",
        "//iamf/cli/codec:lpcm_decoder",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
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
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_decoder_test",
    srcs = ["opus_decoder_test.cc"],
    deps = [
        "//iamf/cli/codec:decoder_base",
        "//iamf/cli/codec:opus_decoder",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_encoder_test",
    srcs = ["opus_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf/cli/codec:opus_encoder",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:codec_config",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
        "@libopus",
    ],
)

# keep-sorted end
