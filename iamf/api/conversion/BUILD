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
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "mix_presentation_conversion",
    srcs = ["mix_presentation_conversion.cc"],
    hdrs = ["mix_presentation_conversion.h"],
    deps = [
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:mix_presentation",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "profile_conversion",
    srcs = ["profile_conversion.cc"],
    hdrs = ["profile_conversion.h"],
    deps = [
        "//iamf/include/iamf_tools:iamf_tools_api_types",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# keep-sorted end
