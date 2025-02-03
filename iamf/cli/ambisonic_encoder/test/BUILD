cc_test(
    name = "ambisonic_encoder_test",
    srcs = ["ambisonic_encoder_test.cc"],
    deps = [
        "//iamf/cli/ambisonic_encoder",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "associated_legendre_polynomials_generator_test",
    srcs = [
        "associated_legendre_polynomials_generator_test.cc",
    ],
    deps = [
        "//iamf/cli/ambisonic_encoder:ambisonic_utils",
        "//iamf/cli/ambisonic_encoder:associated_legendre_polynomials_generator",
        "@com_google_googletest//:gtest_main",
    ],
)

# Benchmark with
#   `bazel run -c opt :ambisonic_encoder_benchmark -- --benchmark_filter=.`
cc_test(
    name = "ambisonic_encoder_benchmark",
    srcs = ["ambisonic_encoder_benchmark.cc"],
    deps = [
        "//iamf/cli/ambisonic_encoder",
        "@com_google_benchmark//:benchmark_main",
    ],
)
