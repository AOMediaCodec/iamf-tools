load("@rules_python//python:defs.bzl", "py_binary")

package(default_visibility = [
    # TODO(b/400697000): Break up visibility for the api directory.
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
    "//iamf/include/iamf_tools:__subpackages__",
])

# keep-sorted start block=yes prefix_order=filegroup,pytype_strict_binary newline_separated=yes
filegroup(
    name = "fuzz_test_iamf",
    srcs = glob(["iamf/*.iamf"]),
)

filegroup(
    name = "input_wav_files",
    srcs = glob(["*.wav"]),
)

filegroup(
    name = "test_vectors",
    srcs = glob(["test_*.textproto"]),
)

py_binary(
    name = "generate_test_suite",
    srcs = ["generate_test_suite.py"],
    deps = [
        "//iamf/cli/proto:test_vector_metadata_py_pb2",
        "//iamf/cli/proto:user_metadata_py_pb2",
        "@abseil-py//absl:app",
        "@abseil-py//absl/flags",
        "@com_google_protobuf//:protobuf_python",
    ],
)

py_binary(
    name = "generate_test_summary",
    srcs = ["generate_test_summary.py"],
    deps = [
        "//iamf/cli/proto:user_metadata_py_pb2",
        "@abseil-py//absl:app",
        "@abseil-py//absl/flags",
        "@abseil-py//absl/logging",
        "@com_google_protobuf//:protobuf_python",
    ],
)

# keep-sorted end
