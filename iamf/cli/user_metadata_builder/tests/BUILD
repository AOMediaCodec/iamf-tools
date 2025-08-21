load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "audio_element_metadata_builder_test",
    srcs = ["audio_element_metadata_builder_test.cc"],
    deps = [
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/user_metadata_builder:audio_element_metadata_builder",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_frame_metadata_builder_test",
    srcs = ["audio_frame_metadata_builder_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto_conversion:channel_label_utils",
        "//iamf/cli/user_metadata_builder:audio_frame_metadata_builder",
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "codec_config_obu_metadata_builder_test",
    srcs = ["codec_config_obu_metadata_builder_test.cc"],
    deps = [
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto_conversion/proto_to_obu:codec_config_generator",
        "//iamf/cli/user_metadata_builder:codec_config_obu_metadata_builder",
        "//iamf/obu:codec_config",
        "//iamf/obu:types",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "iamf_input_layout_test",
    srcs = ["iamf_input_layout_test.cc"],
    deps = [
        "//iamf/cli/user_metadata_builder:iamf_input_layout",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
