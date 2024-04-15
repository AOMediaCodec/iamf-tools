package(default_visibility = ["//iamf/cli:__subpackages__"])

cc_library(
    name = "audio_element_renderer_base",
    srcs = ["audio_element_renderer_base.cc"],
    hdrs = ["audio_element_renderer_base.h"],
    deps = [
        "//iamf/cli:demixing_module",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "audio_element_renderer_passthrough",
    srcs = ["audio_element_renderer_passthrough.cc"],
    hdrs = ["audio_element_renderer_passthrough.h"],
    deps = [
        ":audio_element_renderer_base",
        "//iamf/cli:demixing_module",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)
