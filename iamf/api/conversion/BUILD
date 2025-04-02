package(default_visibility = [
    "//iamf/api/conversion/tests:__pkg__",
    "//iamf/api/decoder:__pkg__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "mix_presentation_conversion",
    srcs = ["mix_presentation_conversion.cc"],
    hdrs = ["mix_presentation_conversion.h"],
    deps = [
        "//iamf/api:iamf_tools_api_types",
        "//iamf/obu:mix_presentation",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
# keep-sorted end
