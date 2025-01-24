# Classes to convert the internal C++ representation of OBUs to the proto representation.
package(default_visibility = [
    "//iamf/cli:__subpackages__",
])

cc_library(
    name = "ia_sequence_header_metadata_generator",
    srcs = ["ia_sequence_header_metadata_generator.cc"],
    hdrs = ["ia_sequence_header_metadata_generator.h"],
    deps = [
        ":obu_header_metadata_generator",
        "//iamf/cli:lookup_tables",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:ia_sequence_header",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "obu_header_metadata_generator",
    srcs = ["obu_header_metadata_generator.cc"],
    hdrs = ["obu_header_metadata_generator.h"],
    deps = [
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:obu_header",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

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
        "//iamf/common:validation_utils",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
    ],
)
