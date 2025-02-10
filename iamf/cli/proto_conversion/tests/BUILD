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
    name = "proto_utils_test",
    srcs = ["proto_utils_test.cc"],
    deps = [
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto_conversion:proto_utils",
        "//iamf/common:leb_generator",
        "//iamf/obu:obu_header",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

# keep-sorted end
