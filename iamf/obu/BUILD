load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//iamf:__subpackages__"])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "arbitrary_obu",
    srcs = [
        "arbitrary_obu.cc",
    ],
    hdrs = ["arbitrary_obu.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "audio_element",
    srcs = ["audio_element.cc"],
    hdrs = ["audio_element.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "audio_frame",
    srcs = [
        "audio_frame.cc",
    ],
    hdrs = ["audio_frame.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "codec_config",
    srcs = ["codec_config.cc"],
    hdrs = ["codec_config.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
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
    name = "ia_sequence_header",
    srcs = ["ia_sequence_header.cc"],
    hdrs = ["ia_sequence_header.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mix_presentation",
    srcs = ["mix_presentation.cc"],
    hdrs = ["mix_presentation.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:map_utils",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "obu_base",
    srcs = ["obu_base.cc"],
    hdrs = ["obu_base.h"],
    deps = [
        ":obu_header",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "obu_header",
    srcs = ["obu_header.cc"],
    hdrs = ["obu_header.h"],
    deps = [
        ":types",
        "//iamf/common:leb_generator",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "param_definitions",
    srcs = [
        "demixing_param_definition.cc",
        "param_definitions.cc",
    ],
    hdrs = [
        "demixing_param_definition.h",
        "param_definition_variant.h",
        "param_definitions.h",
    ],
    deps = [
        ":parameter_data",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:numeric_utils",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "parameter_block",
    srcs = ["parameter_block.cc"],
    hdrs = ["parameter_block.h"],
    deps = [
        ":obu_base",
        ":obu_header",
        ":param_definitions",
        ":parameter_data",
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:obu_util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "parameter_data",
    srcs = [
        "demixing_info_parameter_data.cc",
        "extension_parameter_data.cc",
        "mix_gain_parameter_data.cc",
        "recon_gain_info_parameter_data.cc",
    ],
    hdrs = [
        "demixing_info_parameter_data.h",
        "extension_parameter_data.h",
        "mix_gain_parameter_data.h",
        "parameter_data.h",
        "recon_gain_info_parameter_data.h",
    ],
    deps = [
        ":types",
        "//iamf/common:read_bit_buffer",
        "//iamf/common:write_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/common/utils:validation_utils",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
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
        "//iamf/common/utils:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "types",
    hdrs = ["types.h"],
)

# keep-sorted end
