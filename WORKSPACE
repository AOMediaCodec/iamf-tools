########################
# Platform Independent #
########################

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",  # @unused
)
load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",  # @unused
)

# GoogleTest/GoogleMock framework.
git_repository(
    name = "com_google_googletest",
    remote = "https://github.com/google/googletest.git",
    tag = "v1.14.0",
)

git_repository(
    name = "rules_python",
    remote = "https://github.com/bazelbuild/rules_python.git",
    tag = "0.35.0",
)

load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()

# proto_library, cc_proto_library, and java_proto_library rules implicitly
# depend on @com_google_protobuf for protoc and proto runtimes.
# This statement defines the @com_google_protobuf repo.
git_repository(
    name = "com_google_protobuf",
    remote = "https://github.com/protocolbuffers/protobuf.git",
    tag = "v27.5",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Google Abseil Libraries.
git_repository(
    name = "com_google_absl",
    commit = "7e149e40c7a2d8049ecd28d1f83f64cc197cc128",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

# Google Audio-to-Tactile Library.
git_repository(
    name = "com_google_audio_to_tactile",
    commit = "d3f449fdfd8cfe4a845d0ae244fce2a0bca34a15",
    remote = "https://github.com/google/audio-to-tactile.git",
)

# Google Benchmark Library.
git_repository(
    name = "com_google_benchmark",
    remote = "https://github.com/google/benchmark.git",
    tag = "v1.9.0",
)

# Expat.
git_repository(
    name = "libexpat",
    build_file = "libexpat.BUILD",
    commit = "fa75b96546c069d17b8f80d91e0f4ef0cde3790d",
    remote = "https://github.com/libexpat/libexpat.git",
)

# FDK AAC.
git_repository(
    name = "fdk_aac",
    build_file = "fdk_aac.BUILD",
    commit = "38c27d428ee223bf32f0a2a07cae9fcb99cf3cae",
    remote = "https://android.googlesource.com/platform/external/aac",
)

# FLAC.
git_repository(
    name = "flac",
    build_file = "flac.BUILD",
    commit = "28e4f0528c76b296c561e922ba67d43751990599",
    remote = "https://github.com/xiph/flac.git",
)

# Opus.
git_repository(
    name = "libopus",
    build_file = "libopus.BUILD",
    commit = "82ac57d9f1aaf575800cf17373348e45b7ce6c0d",
    remote = "https://gitlab.xiph.org/xiph/opus.git",
)

# Eigen.
git_repository(
    name = "eigen3",
    build_file = "eigen.BUILD",
    commit = "3147391d946bb4b6c68edd901f2add6ac1f31f8c",
    remote = "https://gitlab.com/libeigen/eigen.git",
)
