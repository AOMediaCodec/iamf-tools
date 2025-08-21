load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "adm_to_user_metadata_main_lib_test",
    srcs = ["adm_to_user_metadata_main_lib_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/app:adm_to_user_metadata_main_lib",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
