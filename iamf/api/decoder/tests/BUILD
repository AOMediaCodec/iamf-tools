load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "iamf_decoder_fuzz_test",
    srcs = ["iamf_decoder_fuzz_test.cc"],
    args = [
        "--fuzztest_rss_limit_mb=8092",
    ],
    data = [
        "//iamf/cli/testdata:fuzz_test_iamf",
    ],
    shard_count = 2,
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "//iamf/api/decoder:iamf_decoder",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "@abseil-cpp//absl/strings:string_view",
        "@fuzztest//fuzztest",
        "@googletest//:gtest_main",
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
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)
# keep-sorted end
