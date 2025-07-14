load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//iamf/cli/renderer/tests:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_test newline_separated=yes
cc_test(
    name = "audio_element_renderer_ambisonics_to_channel_test",
    srcs = ["audio_element_renderer_ambisonics_to_channel_test.cc"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:audio_element_renderer_ambisonics_to_channel",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_base_test",
    srcs = ["audio_element_renderer_base_test.cc"],
    deps = [
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_channel_to_channel_test",
    srcs = ["audio_element_renderer_channel_to_channel_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:audio_element_renderer_channel_to_channel",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_passthrough_test",
    srcs = ["audio_element_renderer_passthrough_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:audio_element_renderer_passthrough",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "loudspeakers_renderer_test",
    srcs = ["loudspeakers_renderer_test.cc"],
    deps = [
        "//iamf/cli/renderer:loudspeakers_renderer",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "renderer_utils_benchmark",
    srcs = ["renderer_utils_benchmark.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:renderer_utils",
        "//iamf/obu:types",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/types:span",
        "@com_google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "renderer_utils_test",
    srcs = ["renderer_utils_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:renderer_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

# keep-sorted end
