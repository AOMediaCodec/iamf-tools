cc_library(
    name = "ambisonic_encoder",
    srcs = [
        "ambisonic_encoder.cc",
    ],
    hdrs = ["ambisonic_encoder.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ambisonic_utils",
        ":associated_legendre_polynomials_generator",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:check",
        "@eigen3",
    ],
)

cc_library(
    name = "ambisonic_utils",
    srcs = [],
    hdrs = ["ambisonic_utils.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/log:check"],
)

cc_library(
    name = "associated_legendre_polynomials_generator",
    srcs = [
        "associated_legendre_polynomials_generator.cc",
    ],
    hdrs = [
        "associated_legendre_polynomials_generator.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":ambisonic_utils",
        "@com_google_absl//absl/log:check",
    ],
)
