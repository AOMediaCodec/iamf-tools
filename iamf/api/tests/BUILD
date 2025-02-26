# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "iamf_decoder_test",
    srcs = ["iamf_decoder_test.cc"],
    deps = [
        "//iamf/api:iamf_decoder",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_frame",
        "//iamf/obu:codec_config",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:obu_header",
        "//iamf/obu:temporal_delimiter",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
