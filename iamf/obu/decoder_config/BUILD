package(default_visibility = ["//iamf:__subpackages__"])

cc_library(
    name = "aac_decoder_config",
    srcs = ["aac_decoder_config.cc"],
    hdrs = ["aac_decoder_config.h"],
    deps = [
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "flac_decoder_config",
    srcs = ["flac_decoder_config.cc"],
    hdrs = ["flac_decoder_config.h"],
    deps = [
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lpcm_decoder_config",
    srcs = ["lpcm_decoder_config.cc"],
    hdrs = ["lpcm_decoder_config.h"],
    deps = [
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "opus_decoder_config",
    srcs = ["opus_decoder_config.cc"],
    hdrs = ["opus_decoder_config.h"],
    deps = [
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
