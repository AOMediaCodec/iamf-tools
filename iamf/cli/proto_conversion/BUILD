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
