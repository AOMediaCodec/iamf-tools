load("@rules_cc//cc:cc_library.bzl", "cc_library")

# Classes to convert the internal C++ representation of OBUs to the proto representation.
package(default_visibility = [
    "//iamf/cli:__subpackages__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "ia_sequence_header_metadata_generator",
    srcs = ["ia_sequence_header_metadata_generator.cc"],
    hdrs = ["ia_sequence_header_metadata_generator.h"],
    deps = [
        ":obu_header_metadata_generator",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto_conversion:lookup_tables",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/functional:function_ref",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "obu_header_metadata_generator",
    srcs = ["obu_header_metadata_generator.cc"],
    hdrs = ["obu_header_metadata_generator.h"],
    deps = [
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:obu_header",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "parameter_block_metadata_generator",
    srcs = ["parameter_block_metadata_generator.cc"],
    hdrs = ["parameter_block_metadata_generator.h"],
    deps = [
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto_conversion:proto_utils",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_block",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@abseil-cpp//absl/status:statusor",
    ],
)

# keep-sorted end
