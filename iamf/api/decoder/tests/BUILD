load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "iamf_decoder_fuzz_test",
    srcs = ["iamf_decoder_fuzz_test.cc"],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "@com_google_fuzztest//fuzztest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iamf_decoder_test",
    srcs = ["iamf_decoder_test.cc"],
    deps = [
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_base",
        "//iamf/obu:obu_header",
        "//iamf/obu:temporal_delimiter",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
# keep-sorted end

cc_test(
    name = "iamf_decoder_factory_test",
    srcs = ["iamf_decoder_factory_test.cc"],
    deps = [
        "//iamf/api/decoder:iamf_decoder_factory",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli/tests:cli_test_utils",
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
