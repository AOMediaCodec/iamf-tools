load("@rules_cc//cc:cc_library.bzl", "cc_library")

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
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
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:check",
        "@eigen",
    ],
)

cc_library(
    name = "ambisonic_utils",
    srcs = [],
    hdrs = ["ambisonic_utils.h"],
    visibility = ["//visibility:public"],
    deps = ["@abseil-cpp//absl/log:check"],
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
        "@abseil-cpp//absl/log:check",
    ],
)

# keep-sorted end
