load("@rules_python//python:defs.bzl", "py_binary")

package(default_visibility = ["//visibility:public"])

py_binary(
    name = "iamf_verifier",
    srcs = ["iamf_verifier.py"],
    data = [
        "//iamf/cli:decoder_main",
        "//validation/iamf_loudness_comparator:iamf_loudness_comparator_main",
        "//validation/opus_hoa:opus_hoa_main",
    ],
    main = "iamf_verifier.py",
    deps = [
        "//validation/mlsd_comparator",
        "@abseil-py//absl:app",
        "@abseil-py//absl/flags",
        "@abseil-py//absl/logging",
    ],
)
