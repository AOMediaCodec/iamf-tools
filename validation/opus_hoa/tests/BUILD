load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "opus_hoa_test",
    srcs = ["opus_hoa_test.cc"],
    deps = [
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:ambisonics_config",
        "//iamf/obu:audio_element",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "//validation/opus_hoa",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
        "@libopus",
    ],
)
