cc_test(
    name = "parameter_block_metadata_generator_test",
    srcs = ["parameter_block_metadata_generator_test.cc"],
    deps = [
        "//iamf/cli/obu_to_proto:parameter_block_metadata_generator",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/obu:demixing_info_param_data",
        "//iamf/obu:leb128",
        "//iamf/obu:parameter_block",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)
