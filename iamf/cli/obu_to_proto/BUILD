package(default_visibility = [
    "//iamf/cli:__subpackages__",
])

cc_library(
    name = "parameter_block_metadata_generator",
    srcs = ["parameter_block_metadata_generator.cc"],
    hdrs = ["parameter_block_metadata_generator.h"],
    deps = [
        "//iamf/cli:cli_util",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
    ],
)
