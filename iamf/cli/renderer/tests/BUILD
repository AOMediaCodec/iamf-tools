package(default_visibility = ["//iamf/cli/renderer/tests:__subpackages__"])

cc_test(
    name = "audio_element_renderer_base_test",
    srcs = ["audio_element_renderer_base_test.cc"],
    deps = [
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:audio_element_renderer_base",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
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
    name = "renderer_utils_test",
    srcs = ["renderer_utils_test.cc"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:renderer_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
