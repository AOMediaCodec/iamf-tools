LIBEXPAT_HDRS = [
    "expat/lib/ascii.h",
    "expat/lib/asciitab.h",
    "expat/lib/expat.h",
    "expat/lib/expat_external.h",
    "expat/lib/iasciitab.h",
    "expat/lib/internal.h",
    "expat/lib/latin1tab.h",
    "expat/lib/nametab.h",
    "expat/lib/siphash.h",
    "expat/lib/utf8tab.h",
    "expat/lib/winconfig.h",
    "expat/lib/xmlrole.h",
    "expat/lib/xmltok.h",
    "expat_config.h",
    "expat/lib/xmltok_impl.c",
    "expat/lib/xmltok_impl.h",
    "expat/lib/xmltok_ns.c",
]

LIBEXPAT_SRCS = [
    "expat/lib/xmlparse.c",
    "expat/lib/xmlrole.c",
    "expat/lib/xmltok.c",
]

CMAKE_DEPS = [
    "expat/CMakeLists.txt",
    "expat/Changes",
    "expat/ConfigureChecks.cmake",
    "expat/expat_config.h.cmake",
    "expat/cmake/expat-config.cmake.in",
]

genrule(
    name = "expat_config",
    srcs = CMAKE_DEPS + LIBEXPAT_SRCS,
    outs = ["expat_config.h"],
    cmd = "cmake $(location expat/CMakeLists.txt) -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_TESTS=OFF \
    -DEXPAT_BUILD_EXAMPLES=OFF -DEXPAT_ENABLE_INSTALL=OFF -DEXPAT_BUILD_PKGCONFIG=OFF; \
    cp expat_config.h $@",
)

cc_library(
    name = "libexpat",
    srcs = LIBEXPAT_SRCS,
    hdrs = LIBEXPAT_HDRS,
    copts = ["-w"],
    defines = [
        "XML_STATIC",
        "XML_GE",
    ],
    # Internally `libexpat` does not qualify any headers.
    includes = [
        ".",
        "lib",
    ],
    visibility = ["//visibility:public"],
)
