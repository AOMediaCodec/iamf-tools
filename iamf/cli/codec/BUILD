# Files to interface between IAMF Tools and codec libraries when encoding and decoding substreams.

package(default_visibility = ["//iamf/cli:__subpackages__"])

cc_library(
    name = "aac_encoder_decoder",
    srcs = ["aac_encoder_decoder.cc"],
    hdrs = ["aac_encoder_decoder.h"],
    deps = [
        ":decoder_base",
        ":encoder_base",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@fdk_aac//:aac_decoder_lib",
        "@fdk_aac//:aac_encoder_lib",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "decoder_base",
    hdrs = ["decoder_base.h"],
    deps = ["@com_google_absl//absl/status"],
)

cc_library(
    name = "encoder_base",
    srcs = ["encoder_base.cc"],
    hdrs = ["encoder_base.h"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/common:macros",
        "//iamf/obu:codec_config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "flac_encoder_decoder",
    srcs = ["flac_encoder_decoder.cc"],
    hdrs = ["flac_encoder_decoder.h"],
    deps = [
        ":encoder_base",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@flac//:src",
    ],
)

cc_library(
    name = "lpcm_decoder",
    srcs = ["lpcm_decoder.cc"],
    hdrs = ["lpcm_decoder.h"],
    deps = [
        ":decoder_base",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "lpcm_encoder",
    srcs = ["lpcm_encoder.cc"],
    hdrs = ["lpcm_encoder.h"],
    deps = [
        ":encoder_base",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:cli_util",
        "//iamf/common:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "opus_encoder_decoder",
    srcs = ["opus_encoder_decoder.cc"],
    hdrs = ["opus_encoder_decoder.h"],
    deps = [
        ":decoder_base",
        ":encoder_base",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@libopus",
    ],
)
