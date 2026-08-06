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
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:ambisonics_config",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "audio_element_renderer_base",
    srcs = ["audio_element_renderer_base.cc"],
    hdrs = ["audio_element_renderer_base.h"],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli:labeled_frame",
        "//iamf/cli:sample_processing_utils",
        "//iamf/common/utils:macros",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "audio_element_renderer_binaural",
    srcs = ["audio_element_renderer_binaural.cc"],
    hdrs = ["audio_element_renderer_binaural.h"],
    deps = [
        ":audio_element_renderer_base",
        ":renderer_utils",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:ambisonics_config",
        "//iamf/obu:audio_element",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
        "@com_google_obr//obr/audio_buffer",
        "@com_google_obr//obr/renderer:audio_element_type",
        "@com_google_obr//obr/renderer:obr_impl",
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
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/log:absl_vlog_is_on",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
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
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "default_layout_renderer",
    srcs = ["default_layout_renderer.cc"],
    hdrs = ["default_layout_renderer.h"],
    deps = [
        ":audio_element_renderer_base",
        ":layout_renderer_base",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:demixing_manager",
        "//iamf/cli:labeled_frame",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli:renderer_factory",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:parameter_block",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "layout_renderer_base",
    srcs = ["layout_renderer_base.cc"],
    hdrs = ["layout_renderer_base.h"],
    deps = [
        "//iamf/cli:demixing_manager",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/obu:types",
        "//iamf/obu/param_definitions:mix_gain_param_definition",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/cli/renderer/gains:get_gains",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "renderer_utils",
    srcs = ["renderer_utils.cc"],
    hdrs = ["renderer_utils.h"],
    deps = [
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:channel_label",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:ambisonics_config",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
    ],
)

# keep-sorted end
