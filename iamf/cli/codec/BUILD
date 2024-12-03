# Files to interface between IAMF Tools and codec libraries when encoding and decoding substreams.

package(default_visibility = ["//iamf/cli:__subpackages__"])

cc_library(
    name = "aac_decoder",
    srcs = ["aac_decoder.cc"],
    hdrs = ["aac_decoder.h"],
    deps = [
        ":aac_utils",
        ":decoder_base",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@fdk_aac//:aac_decoder_lib",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "aac_encoder",
    srcs = ["aac_encoder.cc"],
    hdrs = ["aac_encoder.h"],
    deps = [
        ":aac_utils",
        ":encoder_base",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@fdk_aac//:aac_encoder_lib",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "aac_utils",
    srcs = [],
    hdrs = ["aac_utils.h"],
    deps = ["@fdk_aac//:fdk_sys_lib"],
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
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "flac_encoder",
    srcs = ["flac_encoder.cc"],
    hdrs = ["flac_encoder.h"],
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
        "@com_google_absl//absl/strings",
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
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
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
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "opus_decoder",
    srcs = ["opus_decoder.cc"],
    hdrs = ["opus_decoder.h"],
    deps = [
        ":decoder_base",
        ":opus_utils",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@libopus",
    ],
)

cc_library(
    name = "opus_encoder",
    srcs = ["opus_encoder.cc"],
    hdrs = ["opus_encoder.h"],
    deps = [
        ":encoder_base",
        ":opus_utils",
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

cc_library(
    name = "opus_utils",
    srcs = ["opus_utils.cc"],
    hdrs = ["opus_utils.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@libopus",
    ],
)

cc_library(
    name = "flac_decoder",
    srcs = ["flac_decoder.cc"],
    hdrs = ["flac_decoder.h"],
    deps = [
        ":decoder_base",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@flac//:src",
    ],
)

cc_library(
    name = "flac_decoder_stream_callbacks",
    srcs = ["flac_decoder_stream_callbacks.cc"],
    hdrs = ["flac_decoder_stream_callbacks.h"],
    deps = [
        ":flac_decoder",
        "@com_google_absl//absl/log",
        "@flac//:src",
    ],
)
