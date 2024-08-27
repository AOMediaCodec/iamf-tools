package(
    default_visibility = ["//visibility:public"],
)

licenses(["by_exception_only"])

exports_files(["LICENSE"])

cc_library(
    name = "fdk_sys_lib",
    srcs = glob([
        "libSYS/src/*.cpp",
        "libSYS/src/*.h",
    ]),
    hdrs = glob([
        "libSYS/include/*.h",
    ]),
    includes = [
        "libSYS/include",
    ],
)

ARM_LIB_FDK_HEADERS = glob([
    "libFDK/include/arm/*.h",
    "libFDK/src/arm/*.cpp",
])

ARM_LIB_FDK_INCLUDES = [
    "libFDK/src",
]

X86_LIB_FDK_HEADERS = glob(["libFDK/include/x86/*.h"])

cc_library(
    name = "fdk_core_lib",
    srcs = glob([
        "libFDK/src/*.cpp",
        "libFDK/src/*.h",
    ]),
    hdrs = glob([
        "libFDK/include/*.h",
    ]) + select({
        "@platforms//cpu:arm": ARM_LIB_FDK_HEADERS,
        "@platforms//cpu:arm64": ARM_LIB_FDK_HEADERS,
        "@platforms//cpu:x86_32": X86_LIB_FDK_HEADERS,
        "@platforms//cpu:x86_64": X86_LIB_FDK_HEADERS,
    }),
    includes = [
        "libFDK/include",
        "libSYS/include",
    ] + select({
        "@platforms//cpu:arm": ARM_LIB_FDK_INCLUDES,
        "@platforms//cpu:arm64": ARM_LIB_FDK_INCLUDES,
        "//conditions:default": [],
    }),
    deps = [
        ":fdk_sys_lib",
    ],
)

cc_library(
    name = "mpeg_tp_dec_lib",
    srcs = glob([
        "libMpegTPDec/src/*.cpp",
        "libMpegTPDec/src/*.h",
    ]),
    hdrs = glob([
        "libMpegTPDec/include/*.h",
    ]),
    copts = select({
    "//tools/cc_target_os:windows": [],
    "//conditions:default": [
        "-Wno-implicit-fallthrough",
        "-Wno-unused-label",
        "-Wno-unused-variable",
    ],
    }),
    includes = [
        "libFDK/include",
        "libMpegTPDec/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "mpeg_tp_enc_lib",
    srcs = glob([
        "libMpegTPEnc/src/*.cpp",
        "libMpegTPEnc/src/*.h",
    ]),
    hdrs = glob([
        "libMpegTPEnc/include/*.h",
    ]),
    includes = [
        "libFDK/include",
        "libMpegTPEnc/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "sbrdec_lib",
    srcs = glob([
        "libSBRdec/src/*.cpp",
        "libSBRdec/src/*.h",
    ]),
    hdrs = glob([
        "libSBRdec/include/*.h",
    ]),
    includes = [
        "libFDK/include",
        "libSBRdec/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "sbrenc_lib",
    srcs = glob([
        "libSBRenc/src/*.cpp",
        "libSBRenc/src/*.h",
    ]),
    hdrs = glob([
        "libSBRenc/include/*.h",
    ]),
    includes = [
        "libFDK/include",
        "libSBRenc/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "drcdec_lib",
    srcs = glob([
        "libDRCdec/src/*.cpp",
        "libDRCdec/src/*.h",
    ]),
    hdrs = glob([
        "libDRCdec/include/*.h",
    ]),
    includes = [
        "libDRCdec/include",
        "libFDK/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "sacdec_lib",
    srcs = glob([
        "libSACdec/src/*.cpp",
        "libSACdec/src/*.h",
    ]),
    hdrs = glob([
        "libSACdec/include/*.h",
    ]),
    includes = [
        "libFDK/include",
        "libSACdec/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "sacenc_lib",
    srcs = glob([
        "libSACenc/src/*.cpp",
        "libSACenc/src/*.h",
    ]),
    hdrs = glob([
        "libSACenc/include/*.h",
    ]),
    includes = [
        "libFDK/include",
        "libSACenc/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "pcmutils_lib",
    srcs = glob([
        "libPCMutils/src/*.cpp",
    ]),
    hdrs = glob([
        "libPCMutils/src/*.h",
        "libPCMutils/include/*.h",
    ]),
    copts = select({
    "//tools/cc_target_os:windows": [],
    "//conditions:default": [
        "-Wno-implicit-fallthrough",
        "-Wno-unused-label",
        "-Wno-unused-variable",
    ]}),
    includes = [
        "libFDK/include",
        "libPCMutils/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

cc_library(
    name = "arith_coding_lib",
    srcs = glob([
        "libArithCoding/src/*.cpp",
        "libArithCoding/src/*.h",
    ]),
    hdrs = glob([
        "libArithCoding/include/*.h",
    ]),
    includes = [
        "libArithCoding/include",
        "libFDK/include",
    ],
    deps = [
        ":fdk_core_lib",
    ],
)

ARM_LIB_AAC_DEC_HEADERS = glob([
    "libAACdec/src/arm/*.cpp",
])

ARM_LIB_AAC_DEC_INCLUDES = [
    "libAACdec/src",
]

cc_library(
    name = "aac_decoder_lib",
    srcs = glob([
        "libAACdec/src/*.h",
        "libAACdec/src/*.cpp",
        "libAACdec/include/*.h",
    ]),
    hdrs = glob([
        "libAACdec/include/*.h",
    ]) + select({
        "@platforms//cpu:arm": ARM_LIB_AAC_DEC_HEADERS,
        "@platforms//cpu:arm64": ARM_LIB_AAC_DEC_HEADERS,
        "//conditions:default": [],
    }),
    copts = select({
    "//tools/cc_target_os:windows": [],
    "//conditions:default": [
        "-Wno-implicit-fallthrough",
        "-Wno-unused-variable",
    ]}),
    includes = [
        "libAACdec/include",
        "libArithCoding/include",
        "libDRCdec/include",
        "libFDK/include",
        "libMpegTPEnc/include",
        "libPCMutils/include",
        "libSACdec/include",
        "libSACenc/include",
        "libSBRdec/include",
    ] + select({
        "@platforms//cpu:arm": ARM_LIB_AAC_DEC_INCLUDES,
        "@platforms//cpu:arm64": ARM_LIB_AAC_DEC_INCLUDES,
        "//conditions:default": [],
    }),
    deps = [
        ":arith_coding_lib",
        ":drcdec_lib",
        ":fdk_core_lib",
        ":mpeg_tp_dec_lib",
        ":pcmutils_lib",
        ":sacdec_lib",
        ":sacenc_lib",
        ":sbrdec_lib",
    ],
)

cc_library(
    name = "aac_encoder_lib",
    srcs = glob([
        "libAACenc/src/*.h",
        "libAACenc/src/*.cpp",
        "libAACenc/include/*.h",
    ]),
    hdrs = glob([
        "libAACenc/include/*.h",
    ]),
    includes = [
        "libAACenc/include",
        "libFDK/include",
        "libMpegTPEnc/include",
        "libPCMutils/include",
        "libSACenc/include",
    ],
    deps = [
        ":fdk_core_lib",
        ":mpeg_tp_dec_lib",
        ":mpeg_tp_enc_lib",
        ":pcmutils_lib",
        ":sacenc_lib",
        ":sbrenc_lib",
    ],
)
