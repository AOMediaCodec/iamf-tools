package(default_visibility = ["//:__subpackages__"])

cc_library(
    name = "aac_decoder_config",
    srcs = ["aac_decoder_config.cc"],
    hdrs = ["aac_decoder_config.h"],
    deps = [
        ":ia",
        ":obu_util",
        ":write_bit_buffer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@fdk_aac//:fdk_sys_lib",
    ],
)

cc_library(
    name = "arbitrary_obu",
    srcs = [
        "arbitrary_obu.cc",
    ],
    hdrs = ["arbitrary_obu.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "audio_element",
    srcs = ["audio_element.cc"],
    hdrs = ["audio_element.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":obu_util",
        ":param_definitions",
        ":write_bit_buffer",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "audio_frame",
    srcs = [
        "audio_frame.cc",
    ],
    hdrs = ["audio_frame.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "codec_config",
    srcs = ["codec_config.cc"],
    hdrs = ["codec_config.h"],
    deps = [
        ":aac_decoder_config",
        ":flac_decoder_config",
        ":ia",
        ":lpcm_decoder_config",
        ":obu_base",
        ":obu_header",
        ":opus_decoder_config",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "demixing_info_param_data",
    srcs = ["demixing_info_param_data.cc"],
    hdrs = ["demixing_info_param_data.h"],
    deps = [
        ":ia",
        ":write_bit_buffer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "flac_decoder_config",
    srcs = ["flac_decoder_config.cc"],
    hdrs = ["flac_decoder_config.h"],
    deps = [
        ":ia",
        ":obu_util",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "ia",
    hdrs = ["ia.h"],
)

cc_library(
    name = "ia_sequence_header",
    srcs = ["ia_sequence_header.cc"],
    hdrs = ["ia_sequence_header.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":obu_util",
        ":write_bit_buffer",
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
        ":ia",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mix_presentation",
    srcs = ["mix_presentation.cc"],
    hdrs = ["mix_presentation.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":obu_util",
        ":param_definitions",
        ":write_bit_buffer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "obu_base",
    srcs = ["obu_base.cc"],
    hdrs = ["obu_base.h"],
    deps = [
        ":ia",
        ":obu_header",
        ":write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "obu_header",
    srcs = ["obu_header.cc"],
    hdrs = ["obu_header.h"],
    deps = [
        ":ia",
        ":obu_util",
        ":write_bit_buffer",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "obu_util",
    srcs = ["obu_util.cc"],
    hdrs = ["obu_util.h"],
    deps = [
        ":ia",
        "@com_google_absl//absl/container:flat_hash_map",
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
        ":ia",
        ":obu_util",
        ":write_bit_buffer",
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
        ":ia",
        ":obu_util",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "parameter_block",
    srcs = ["parameter_block.cc"],
    hdrs = ["parameter_block.h"],
    deps = [
        ":demixing_info_param_data",
        ":ia",
        ":obu_base",
        ":obu_header",
        ":obu_util",
        ":param_definitions",
        ":write_bit_buffer",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "temporal_delimiter",
    hdrs = ["temporal_delimiter.h"],
    deps = [
        ":ia",
        ":obu_base",
        ":obu_header",
        ":write_bit_buffer",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "write_bit_buffer",
    srcs = ["write_bit_buffer.cc"],
    hdrs = ["write_bit_buffer.h"],
    deps = [
        ":ia",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "read_bit_buffer",
    srcs = ["read_bit_buffer.cc"],
    hdrs = ["read_bit_buffer.h"],
    deps = [
        ":bit_buffer_util",
        ":ia",
        "//iamf/cli:leb_generator",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "bit_buffer_util",
    srcs = ["bit_buffer_util.cc"],
    hdrs = ["bit_buffer_util.h"],
    deps = ["@com_google_absl//absl/status"],
)
