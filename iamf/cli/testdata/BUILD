package(default_visibility = [
    # TODO(b/400697000): Break up visibility for the api directory.
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
    "//iamf/include/iamf_tools:__subpackages__",
])

# keep-sorted start block=yes prefix_order=filegroup newline_separated=yes
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

# keep-sorted end
