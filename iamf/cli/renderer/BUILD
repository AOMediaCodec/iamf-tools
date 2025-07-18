load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf/cli:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "audio_element_renderer_ambisonics_to_channel",
    srcs = ["audio_element_renderer_ambisonics_to_channel.cc"],
    hdrs = ["audio_element_renderer_ambisonics_to_channel.h"],
    deps = [
        ":audio_element_renderer_base",
        ":loudspeakers_renderer",
        ":renderer_utils",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "audio_element_renderer_base",
    srcs = ["audio_element_renderer_base.cc"],
    hdrs = ["audio_element_renderer_base.h"],
    deps = [
        ":renderer_utils",
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "audio_element_renderer_channel_to_channel",
    srcs = ["audio_element_renderer_channel_to_channel.cc"],
    hdrs = ["audio_element_renderer_channel_to_channel.h"],
    deps = [
        ":audio_element_renderer_base",
        ":loudspeakers_renderer",
        ":renderer_utils",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "audio_element_renderer_passthrough",
    srcs = ["audio_element_renderer_passthrough.cc"],
    hdrs = ["audio_element_renderer_passthrough.h"],
    deps = [
        ":audio_element_renderer_base",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "loudspeakers_renderer",
    srcs = [
        "loudspeakers_renderer.cc",
    ],
    hdrs = [
        "loudspeakers_renderer.h",
    ],
    deps = [
        ":precomputed_gains",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:audio_element",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "precomputed_gains",
    srcs = ["precomputed_gains.cc"],
    hdrs = ["precomputed_gains.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_map"],
)

cc_library(
    name = "renderer_utils",
    srcs = ["renderer_utils.cc"],
    hdrs = ["renderer_utils.h"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:demixing_module",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

# keep-sorted end
