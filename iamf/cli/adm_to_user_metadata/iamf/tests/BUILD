cc_test(
    name = "audio_element_handler_test",
    srcs = ["audio_element_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:audio_element_handler",
        "//iamf/cli/adm_to_user_metadata/iamf:iamf_input_layout",
        "//iamf/cli/proto:audio_element_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_frame_handler_test",
    srcs = ["audio_frame_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:audio_frame_handler",
        "//iamf/cli/adm_to_user_metadata/iamf:iamf_input_layout",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "codec_config_obu_metadata_handler_test",
    srcs = ["codec_config_obu_metadata_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/adm_to_user_metadata/iamf:codec_config_obu_metadata_handler",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ia_sequence_header_obu_metadata_handler_test",
    srcs = ["ia_sequence_header_obu_metadata_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:ia_sequence_header_obu_metadata_handler",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/obu:ia_sequence_header",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iamf_test",
    srcs = ["iamf_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/adm_to_user_metadata/iamf",
        "//iamf/cli/adm_to_user_metadata/iamf:iamf_input_layout",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "iamf_input_layout_test",
    srcs = ["iamf_input_layout_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:iamf_input_layout",
        "@com_google_absl//absl/strings:string_view",
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
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "test_vector_metadata_handler_test",
    srcs = ["test_vector_metadata_handler_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/iamf:test_vector_metadata_handler",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@com_google_absl//absl/strings:string_view",
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
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)
