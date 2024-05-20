package(default_visibility = ["//iamf:__subpackages__"])

cc_library(
    name = "arbitrary_obu",
    srcs = [
        "arbitrary_obu.cc",
    ],
    hdrs = ["arbitrary_obu.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        "//iamf/common:macros",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_element",
    srcs = ["audio_element.cc"],
    hdrs = ["audio_element.h"],
    deps = [
        ":leb128",
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "audio_frame",
    srcs = [
        "audio_frame.cc",
    ],
    hdrs = ["audio_frame.h"],
    deps = [
        ":leb128",
        ":obu_base",
        ":obu_header",
        "//iamf/common:macros",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "codec_config",
    srcs = ["codec_config.cc"],
    hdrs = ["codec_config.h"],
    deps = [
        ":leb128",
        ":obu_base",
        ":obu_header",
        "//iamf/common:macros",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/obu/decoder_config:aac_decoder_config",
        "//iamf/obu/decoder_config:flac_decoder_config",
        "//iamf/obu/decoder_config:lpcm_decoder_config",
        "//iamf/obu/decoder_config:opus_decoder_config",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "demixing_info_param_data",
    srcs = ["demixing_info_param_data.cc"],
    hdrs = ["demixing_info_param_data.h"],
    deps = [
        "//iamf/common:macros",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "ia_sequence_header",
    srcs = ["ia_sequence_header.cc"],
    hdrs = ["ia_sequence_header.h"],
    deps = [
        ":obu_base",
        ":obu_header",
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

cc_library(
    name = "leb128",
    hdrs = ["leb128.h"],
)

cc_library(
    name = "mix_presentation",
    srcs = ["mix_presentation.cc"],
    hdrs = ["mix_presentation.h"],
    deps = [
        ":leb128",
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "obu_base",
    srcs = ["obu_base.cc"],
    hdrs = ["obu_base.h"],
    deps = [
        ":obu_header",
        "//iamf/cli:leb_generator",
        "//iamf/common:macros",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "obu_header",
    srcs = ["obu_header.cc"],
    hdrs = ["obu_header.h"],
    deps = [
        ":leb128",
        "//iamf/cli:leb_generator",
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
    name = "param_definitions",
    srcs = ["param_definitions.cc"],
    hdrs = ["param_definitions.h"],
    deps = [
        ":demixing_info_param_data",
        ":leb128",
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
    name = "parameter_block",
    srcs = ["parameter_block.cc"],
    hdrs = ["parameter_block.h"],
    deps = [
        ":demixing_info_param_data",
        ":leb128",
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        "//iamf/common:macros",
        "//iamf/common:obu_util",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "temporal_delimiter",
    hdrs = ["temporal_delimiter.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
