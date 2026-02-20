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
        "@abseil-cpp//absl/container:flat_hash_map",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "iamf_encoder_factory_test",
    srcs = ["iamf_encoder_factory_test.cc"],
    deps = [
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/cli/user_metadata_builder:audio_element_metadata_builder",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "//iamf/include/iamf_tools:iamf_encoder_factory",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
        "@protobuf",
    ],
)

cc_test(
    name = "iamf_encoder_factory_fuzz_test",
    srcs = ["iamf_encoder_factory_fuzz_test.cc"],
    args = [
        "--fuzztest_rss_limit_mb=8092",
    ],
    data = [
        "//iamf/cli/testdata:test_vectors",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    deps = [
        "@googletest//:gtest_main",
        "@fuzztest//fuzztest",
        "@abseil-cpp//absl/strings:string_view",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/include/iamf_tools:iamf_encoder_factory",
        "@protobuf",
        # [internal] Placeholder for fine-grained protobuf dependency: "io",
    ],
)
