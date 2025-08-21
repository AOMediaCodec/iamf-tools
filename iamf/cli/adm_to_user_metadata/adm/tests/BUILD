load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "bw64_reader_test",
    srcs = ["bw64_reader_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:bw64_reader",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wav_file_splicer_test",
    srcs = ["wav_file_splicer_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:bw64_reader",
        "//iamf/cli/adm_to_user_metadata/adm:wav_file_splicer",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "xml_to_adm_test",
    srcs = ["xml_to_adm_test.cc"],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
