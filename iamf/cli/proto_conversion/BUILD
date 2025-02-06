# Conversion of proto to internal representations.
# cli/proto_to_obu and cli/obu_to_proto will be moved here but bidirectional conversion can stay in
# this directory.

cc_library(
    name = "channel_label_utils",
    srcs = ["channel_label_utils.cc"],
    hdrs = ["channel_label_utils.h"],
    visibility = [
        "//iamf/api/google:__pkg__",
        "//iamf/api/google/tests:__pkg__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/internal/transmuxer:__pkg__",
        "//iamf/cli/proto_conversion/proto_to_obu:__pkg__",
        "//iamf/cli/proto_conversion/tests:__pkg__",
        "//iamf/cli/tests:__pkg__",
        "//iamf/cli/user_metadata_builder/tests:__pkg__",
    ],
    deps = [
        "//iamf/cli:channel_label",
        "//iamf/cli/proto:audio_frame_cc_proto",
        "//iamf/common/utils:map_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "proto_utils",
    srcs = ["proto_utils.cc"],
    hdrs = ["proto_utils.h"],
    visibility = [
        "//iamf/api/google:__pkg__",
        "//iamf/cli:__pkg__",
        "//iamf/cli/proto_conversion:__subpackages__",
    ],
    deps = [
        "//iamf/cli:lookup_tables",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:param_definitions_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/common:leb_generator",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:obu_header",
        "//iamf/obu:param_definitions",
        "//iamf/obu:parameter_data",
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)
