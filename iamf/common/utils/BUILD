# Directory for shared utils.  Utils here should not depend on any IAMF code outside of this dir.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "macros",
    hdrs = ["macros.h"],
)

cc_library(
    name = "map_utils",
    hdrs = ["map_utils.h"],
    deps = [
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/functional:function_ref",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_library(
    name = "numeric_utils",
    srcs = ["numeric_utils.cc"],
    hdrs = ["numeric_utils.h"],
    deps = [
        ":validation_utils",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "obu_util",
    hdrs = ["obu_util.h"],
    deps = [
        ":numeric_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "sample_processing_utils",
    srcs = ["sample_processing_utils.cc"],
    hdrs = ["sample_processing_utils.h"],
    deps = [
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "validation_utils",
    hdrs = ["validation_utils.h"],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

# keep-sorted end
