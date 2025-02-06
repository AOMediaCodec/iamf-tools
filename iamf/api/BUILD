package(default_visibility = [
    "//iamf/api:__subpackages__",
])

cc_library(
    name = "iamf_decoder",
    srcs = ["iamf_decoder.cc"],
    hdrs = ["iamf_decoder.h"],
    deps = [
        "//iamf/cli:obu_processor",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)
