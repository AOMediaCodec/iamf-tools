# Files to interface between IAMF Tools and codec libraries when encoding and decoding substreams.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf/cli:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "aac_decoder",
    srcs = ["aac_decoder.cc"],
    hdrs = ["aac_decoder.h"],
    deps = [
        ":aac_utils",
        ":decoder_base",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/synchronization",
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
    deps = [
        "//iamf/obu:types",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_library(
    name = "encoder_base",
    srcs = ["encoder_base.cc"],
    hdrs = ["encoder_base.h"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/common/utils:macros",
        "//iamf/obu:codec_config",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/synchronization",
    ],
)

cc_library(
    name = "flac_decoder",
    srcs = ["flac_decoder.cc"],
    hdrs = ["flac_decoder.h"],
    deps = [
        ":decoder_base",
        ":flac_decoder_stream_callbacks",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@flac//:src",
    ],
)

cc_library(
    name = "flac_decoder_stream_callbacks",
    srcs = ["flac_decoder_stream_callbacks.cc"],
    hdrs = ["flac_decoder_stream_callbacks.h"],
    deps = [
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/types:span",
        "@flac//:src",
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
        "//iamf/common/utils:macros",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/types:span",
        "@flac//:src",
    ],
)

cc_library(
    name = "lpcm_decoder",
    srcs = ["lpcm_decoder.cc"],
    hdrs = ["lpcm_decoder.h"],
    deps = [
        ":decoder_base",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/common/utils:macros",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/synchronization",
    ],
)

cc_library(
    name = "opus_decoder",
    srcs = ["opus_decoder.cc"],
    hdrs = ["opus_decoder.h"],
    deps = [
        ":decoder_base",
        ":opus_utils",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/obu:types",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
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
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:sample_processing_utils",
        "//iamf/common/utils:validation_utils",
        "//iamf/obu:codec_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/types:span",
        "@libopus",
    ],
)

cc_library(
    name = "opus_utils",
    srcs = ["opus_utils.cc"],
    hdrs = ["opus_utils.h"],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
        "@libopus",
    ],
)

# keep-sorted end
