load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = [
    "//iamf/cli/adm_to_user_metadata:__subpackages__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "bw64_reader",
    srcs = ["bw64_reader.cc"],
    hdrs = ["bw64_reader.h"],
    deps = [
        ":format_info_chunk",
        ":xml_to_adm",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "format_info_chunk",
    hdrs = ["format_info_chunk.h"],
)

cc_library(
    name = "panner",
    srcs = ["panner.cc"],
    hdrs = ["panner.h"],
    deps = [
        ":xml_to_adm",
        "//iamf/cli:wav_writer",
        "//iamf/cli/ambisonic_encoder",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/types:span",
        "@com_google_audio_to_tactile//:dsp",
        "@eigen",
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
        ":bw64_reader",
        ":format_info_chunk",
        ":panner",
        ":xml_to_adm",
        "//iamf/cli:wav_writer",
        "//iamf/common/utils:macros",
        "//iamf/obu:ia_sequence_header",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
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
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@libexpat",
    ],
)

# keep-sorted end
