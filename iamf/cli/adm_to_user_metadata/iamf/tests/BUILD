load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "ia_sequence_header_obu_metadata_handler_test",
    srcs = ["ia_sequence_header_obu_metadata_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:ia_sequence_header_obu_metadata_handler",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iamf_test",
    srcs = ["iamf_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/adm_to_user_metadata/iamf",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_handler_test",
    srcs = ["mix_presentation_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/adm_to_user_metadata/iamf:mix_presentation_handler",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_vector_metadata_handler_test",
    srcs = ["test_vector_metadata_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:test_vector_metadata_handler",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "user_metadata_generator_test",
    srcs = ["user_metadata_generator_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/adm_to_user_metadata/iamf:user_metadata_generator",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/tests:cli_test_utils",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
