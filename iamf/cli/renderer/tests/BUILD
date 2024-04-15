package(default_visibility = ["//iamf/cli/renderer/tests:__subpackages__"])

cc_test(
    name = "audio_element_renderer_base_test",
    srcs = ["audio_element_renderer_base_test.cc"],
    deps = [
        "//iamf/cli:demixing_module",
        "//iamf/cli/renderer:audio_element_renderer_base",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_element_renderer_passthrough_test",
    srcs = ["audio_element_renderer_passthrough_test.cc"],
    deps = [
        "//iamf/cli/renderer:audio_element_renderer_passthrough",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "@com_google_googletest//:gtest_main",
    ],
)
