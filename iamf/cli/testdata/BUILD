package(default_visibility = [
    "//iamf/api:__subpackages__",
    "//iamf/cli:__subpackages__",
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
