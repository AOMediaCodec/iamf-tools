load("@rules_cc//cc:cc_test.bzl", "cc_test")

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "channel_reorderer_benchmark",
    srcs = ["channel_reorderer_benchmark.cc"],
    deps = [
        "//iamf/api/conversion:channel_reorderer",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
        "@com_google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "channel_reorderer_test",
    srcs = ["channel_reorderer_test.cc"],
    deps = [
        "//iamf/api/conversion:channel_reorderer",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_conversion_test",
    srcs = ["mix_presentation_conversion_test.cc"],
    deps = [
        "//iamf/api/conversion:mix_presentation_conversion",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:mix_presentation",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "profile_conversion_test",
    srcs = ["profile_conversion_test.cc"],
    deps = [
        "//iamf/api/conversion:profile_conversion",
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
