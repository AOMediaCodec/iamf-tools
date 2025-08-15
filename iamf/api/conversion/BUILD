load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    "//iamf/api/conversion/tests:__pkg__",
    "//iamf/api/decoder:__pkg__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "channel_reorderer",
    srcs = ["channel_reorderer.cc"],
    hdrs = ["channel_reorderer.h"],
    deps = [
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "mix_presentation_conversion",
    srcs = ["mix_presentation_conversion.cc"],
    hdrs = ["mix_presentation_conversion.h"],
    deps = [
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "profile_conversion",
    srcs = ["profile_conversion.cc"],
    hdrs = ["profile_conversion.h"],
    deps = [
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:ia_sequence_header",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

# keep-sorted end
