cc_test(
    name = "codec_config_obu_metadata_builder_test",
    srcs = ["codec_config_obu_metadata_builder_test.cc"],
    deps = [
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto_to_obu:codec_config_generator",
        "//iamf/cli/user_metadata_builder:codec_config_obu_metadata_builder",
        "//iamf/obu:codec_config",
        "//iamf/obu:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
