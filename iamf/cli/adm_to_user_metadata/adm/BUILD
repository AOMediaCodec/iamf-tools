package(default_visibility = [
    "//iamf/cli/adm_to_user_metadata:__subpackages__",
])

cc_library(
    name = "format_info_chunk",
    hdrs = ["format_info_chunk.h"],
)

cc_library(
    name = "bw64_reader",
    srcs = ["bw64_reader.cc"],
    hdrs = ["bw64_reader.h"],
    deps = [
        ":format_info_chunk",
        ":xml_to_adm",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "wav_file_splicer",
    srcs = ["wav_file_splicer.cc"],
    hdrs = ["wav_file_splicer.h"],
    visibility = [
        "//iamf/cli/adm_to_user_metadata/adm/tests:__pkg__",
        "//iamf/cli/adm_to_user_metadata/app:__pkg__",
    ],
    deps = [
        "//iamf/cli:wav_writer",
        "//iamf/cli/adm_to_user_metadata/adm:bw64_reader",
        "//iamf/cli/adm_to_user_metadata/adm:format_info_chunk",
        "//iamf/cli/adm_to_user_metadata/adm:xml_to_adm",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "xml_to_adm",
    srcs = ["xml_to_adm.cc"],
    hdrs = [
        "adm_elements.h",
        "xml_to_adm.h",
    ],
    deps = [
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@libexpat",
    ],
)
