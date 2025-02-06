cc_test(
    name = "ia_sequence_header_metadata_generator_test",
    srcs = ["ia_sequence_header_metadata_generator_test.cc"],
    deps = [
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion/obu_to_proto:ia_sequence_header_metadata_generator",
        "//iamf/cli/proto_conversion/proto_to_obu:ia_sequence_header_generator",
        "//iamf/obu:ia_sequence_header",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "obu_header_metadata_generator_test",
    srcs = ["obu_header_metadata_generator_test.cc"],
    deps = [
        "//iamf/cli:proto_utils",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto_conversion/obu_to_proto:obu_header_metadata_generator",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameter_block_metadata_generator_test",
    srcs = ["parameter_block_metadata_generator_test.cc"],
    deps = [
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion/obu_to_proto:parameter_block_metadata_generator",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)
