package(default_visibility = ["//iamf/cli:__subpackages__"])

cc_library(
    name = "audio_element_renderer_base",
    srcs = ["audio_element_renderer_base.cc"],
    hdrs = ["audio_element_renderer_base.h"],
    deps = [
        "//iamf/cli:demixing_module",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "audio_element_renderer_passthrough",
    srcs = ["audio_element_renderer_passthrough.cc"],
    hdrs = ["audio_element_renderer_passthrough.h"],
    deps = [
        ":audio_element_renderer_base",
        ":renderer_utils",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/common:macros",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "precomputed_gains",
    srcs = ["precomputed_gains.cc"],
    hdrs = ["precomputed_gains.h"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "renderer_utils",
    srcs = ["renderer_utils.cc"],
    hdrs = ["renderer_utils.h"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/common:macros",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
