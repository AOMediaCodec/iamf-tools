# FLAC - Free Lossless Audio Codec

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

flac_version_dir = "src"

exports_files(["LICENSE"])

platform_srcs = select({
    "@platforms//os:windows": glob(["src/share/win_utf8_io/*.c"]),
    "//conditions:default": glob([]),
})

flac_srcs = glob(
    [
        # keep sorted
        "src/libFLAC/*.c",
        "src/libFLAC/**/*.c",
    ],
    exclude = [
        "src/libFLAC/deduplication/*.c",
        "src/libFLAC/deduplication/**/*.c",
        "src/libFLAC/ogg*",
    ],
) + platform_srcs

flac_textual_includes = glob([
    # keep sorted
    "src/libFLAC/deduplication/*.c",
    "src/libFLAC/deduplication/**/*.c",
])

flac_hdrs = glob([
    # keep sorted
    "*.h",
    "include/FLAC/*.h",
    "include/FLAC/**/*.h",
    "include/share/*.h",
    "include/share/**/*.h",
    "src/libFLAC/*.h",
    "src/libFLAC/**/*.h",
])

flac_copts = ["-w"]

flac_includes = [
    "src/libFLAC/include",
    "include",
]

# Defines which do not need to propagate to all downstream users.
flac_local_defines = [
    "FLAC__HAS_OGG=0",
    "FLAC__USE_VISIBILITY_ATTR",
    "PACKAGE_VERSION=\\\"1.5.0\\\"",
    "HAVE_STDINT_H",
    "HAVE_LROUND",
    "NDEBUG",
    "_FORTIFY_SOURCE=2",
] + select({
    "@platforms//os:osx": ["HAVE_FSEEKO"],
    "//conditions:default": [],
})

# Defines which need to propagate to all downstream users.
flac_defines = select({
    "@platforms//os:windows": ["FLAC__NO_DLL"],
    "//conditions:default": [],
})

flac_linkopts = select({
    "@platforms//os:windows": [],
    "//conditions:default": ["-lm"],
})

cc_library(
    name = "flac",
    srcs = flac_srcs,
    hdrs = flac_hdrs,
    copts = flac_copts,
    defines = flac_defines,
    includes = flac_includes,
    linkopts = flac_linkopts,
    local_defines = flac_local_defines,
    textual_hdrs = flac_textual_includes,
)

cc_library(
    name = "src",
    copts = flac_copts,
    deps = [
        ":flac",
    ],
)
