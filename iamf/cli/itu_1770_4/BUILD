load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "loudness_calculator_factory_itu_1770_4",
    srcs = ["loudness_calculator_factory_itu_1770_4.cc"],
    hdrs = ["loudness_calculator_factory_itu_1770_4.h"],
    deps = [
        ":loudness_calculator_itu_1770_4",
        "//iamf/cli:loudness_calculator_base",
        "//iamf/cli:loudness_calculator_factory_base",
        "//iamf/obu:mix_presentation",
    ],
)

cc_library(
    name = "loudness_calculator_itu_1770_4",
    srcs = ["loudness_calculator_itu_1770_4.cc"],
    hdrs = ["loudness_calculator_itu_1770_4.h"],
    deps = [
        "//iamf/cli:loudness_calculator_base",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_loudness_ebur128//:ebur128_analyzer",
    ],
)

# keep-sorted end
