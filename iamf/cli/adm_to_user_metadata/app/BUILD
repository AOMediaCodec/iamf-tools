load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=cc_library,cc_binary, newline_separated=yes
cc_library(
    name = "adm_to_user_metadata_main_lib",
    srcs = ["adm_to_user_metadata_main_lib.cc"],
    hdrs = ["adm_to_user_metadata_main_lib.h"],
    visibility = [
        "//iamf/cli:__pkg__",
        "//iamf/cli/adm_to_user_metadata/app/tests:__pkg__",
    ],
    deps = [
        "//iamf/cli/adm_to_user_metadata/adm:bw64_reader",
        "//iamf/cli/adm_to_user_metadata/adm:wav_file_splicer",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "//iamf/cli/adm_to_user_metadata/iamf:user_metadata_generator",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "//iamf/common/utils:macros",
        "//iamf/obu:ia_sequence_header",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_binary(
    name = "adm_to_user_metadata_main",
    srcs = ["adm_to_user_metadata_main.cc"],
    deps = [
        ":adm_to_user_metadata_main_lib",
        "//iamf/cli/adm_to_user_metadata/iamf:user_metadata_generator",
        "//iamf/obu:ia_sequence_header",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:flags",
    ],
)

# keep-sorted end
