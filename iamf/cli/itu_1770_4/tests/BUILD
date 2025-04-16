# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "loudness_calculator_factory_itu_1770_4_test",
    srcs = ["loudness_calculator_factory_itu_1770_4_test.cc"],
    deps = [
        "//iamf/cli/itu_1770_4:loudness_calculator_factory_itu_1770_4",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/obu:mix_presentation",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "loudness_calculator_itu_1770_4_test",
    srcs = ["loudness_calculator_itu_1770_4_test.cc"],
    deps = [
        "//iamf/cli/itu_1770_4:loudness_calculator_itu_1770_4",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
