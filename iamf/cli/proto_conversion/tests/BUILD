load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "channel_label_utils_test",
    srcs = ["channel_label_utils_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli/proto_conversion:channel_label_utils",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "downmixing_reconstruction_util_test",
    srcs = ["downmixing_reconstruction_util_test.cc"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/proto_conversion:downmixing_reconstruction_util",
        "//iamf/obu:audio_element",
        "//iamf/obu:obu_header",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "output_audio_format_utils_test",
    srcs = ["output_audio_format_utils_test.cc"],
    deps = [
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto_conversion:output_audio_format_utils",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "proto_utils_test",
    srcs = ["proto_utils_test.cc"],
    deps = [
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto_conversion:proto_utils",
        "//iamf/common:leb_generator",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_data",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

# keep-sorted end
