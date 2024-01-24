# FLAC - Free Lossless Audio Codec

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

flac_version_dir = "src"

exports_files(["LICENSE"])

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
)

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

flac_copts = [
    "-w",
    "-DFLAC__USE_VISIBILITY_ATTR",
    "-DPACKAGE_VERSION=\\\"1.4.3\\\"",
    "-DHAVE_STDINT_H",
    "-DHAVE_LROUND",
    "-DNDEBUG",
    "-D_FORTIFY_SOURCE=2",
    "-DFLAC__HAS_OGG=0",
    "-Iexternal/flac/src/libFLAC/include",
    "-Iexternal/flac/include",
]

flac_linkopts = ["-lm"]

cc_library(
    name = "flac",
    srcs = flac_srcs,
    hdrs = flac_hdrs,
    copts = flac_copts,
    linkopts = flac_linkopts,
    textual_hdrs = flac_textual_includes,
)

cc_library(
    name = "src",
    copts = [
        "-w",
    ],
    deps = [
        ":flac",
    ],
)
