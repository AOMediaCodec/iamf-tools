package(default_visibility = [
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
])

filegroup(
    name = "input_wav_files",
    srcs = glob(["*.wav"]),
)

filegroup(
    name = "test_vectors",
    srcs = glob(["test_*.textproto"]),
)

filegroup(
    name = "fuzz_test_iamf",
    srcs = glob(["iamf/*.iamf"]),
)
