load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "iamf_decoder_factory_test",
    srcs = ["iamf_decoder_factory_test.cc"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/include/iamf_tools:iamf_decoder_factory",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)
