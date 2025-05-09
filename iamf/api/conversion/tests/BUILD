load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "channel_reorderer_benchmark",
    srcs = ["channel_reorderer_benchmark.cc"],
    deps = [
        "//iamf/api/conversion:channel_reorderer",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "channel_reorderer_test",
    srcs = ["channel_reorderer_test.cc"],
    deps = [
        "//iamf/api/conversion:channel_reorderer",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_conversion_test",
    srcs = ["mix_presentation_conversion_test.cc"],
    deps = [
        "//iamf/api:iamf_tools_api_types",
        "//iamf/api/conversion:mix_presentation_conversion",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "profile_conversion_test",
    srcs = ["profile_conversion_test.cc"],
    deps = [
        "//iamf/api:iamf_tools_api_types",
        "//iamf/api/conversion:profile_conversion",
        "//iamf/obu:ia_sequence_header",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
