load("@rules_python//python:defs.bzl", "py_binary")

package(default_visibility = ["//visibility:public"])

py_binary(
    name = "iamf_verifier",
    srcs = ["iamf_verifier.py"],
    data = [
        "//validation/iamf_loudness_comparator:iamf_loudness_comparator_main",
    ],
    main = "iamf_verifier.py",
    deps = [
        "@abseil-py//absl:app",
        "@abseil-py//absl/flags",
        "@abseil-py//absl/logging",
    ],
)
