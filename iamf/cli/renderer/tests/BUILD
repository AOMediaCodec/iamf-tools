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
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_base_test",
    srcs = ["audio_element_renderer_base_test.cc"],
    deps = [
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_binaural_test",
    srcs = ["audio_element_renderer_binaural_test.cc"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/cli/renderer:audio_element_renderer_binaural",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
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
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
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
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "loudspeakers_renderer_test",
    srcs = ["loudspeakers_renderer_test.cc"],
    deps = [
        "//iamf/cli/renderer:loudspeakers_renderer",
        "//iamf/obu:parameter_data",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest_main",
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
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/random",
        "@abseil-cpp//absl/types:span",
        "@google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "renderer_utils_test",
    srcs = ["renderer_utils_test.cc"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:renderer_utils",
        "//iamf/cli/tests:cli_test_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

# keep-sorted end
