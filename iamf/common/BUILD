package(default_visibility = ["//iamf:__subpackages__"])

cc_library(
    name = "bit_buffer_util",
    srcs = ["bit_buffer_util.cc"],
    hdrs = ["bit_buffer_util.h"],
    deps = ["@com_google_absl//absl/status"],
)

cc_library(
    name = "macros",
    hdrs = ["macros.h"],
)

cc_library(
    name = "obu_util",
    srcs = ["obu_util.cc"],
    hdrs = ["obu_util.h"],
    deps = [
        ":macros",
        "//iamf/obu:types",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "read_bit_buffer",
    srcs = ["read_bit_buffer.cc"],
    hdrs = ["read_bit_buffer.h"],
    deps = [
        ":bit_buffer_util",
        ":macros",
        "//iamf/obu:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "write_bit_buffer",
    srcs = ["write_bit_buffer.cc"],
    hdrs = ["write_bit_buffer.h"],
    deps = [
        ":bit_buffer_util",
        ":macros",
        "//iamf/cli:leb_generator",
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
