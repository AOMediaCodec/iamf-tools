########################
# Platform Independent #
########################

load("@bazel_tools//tools/build_defs/repo:git.bzl",
     "git_repository",
     "new_git_repository",  # @unused
)
load("@bazel_tools//tools/build_defs/repo:http.bzl",
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
  tag = "0.35.0",
  remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

# proto_library, cc_proto_library, and java_proto_library rules implicitly
# depend on @com_google_protobuf for protoc and proto runtimes.
# This statement defines the @com_google_protobuf repo.
git_repository(
    name = "com_google_protobuf",
    remote = "https://github.com/protocolbuffers/protobuf.git",
    tag = "v27.4",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

# Google Abseil Libs
git_repository(
    name = "com_google_absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "7e149e40c7a2d8049ecd28d1f83f64cc197cc128",
)

# Google Audio-to-Tactile Lib
git_repository(
    name = "com_google_audio_to_tactile",
    remote = "https://github.com/google/audio-to-tactile.git",
    commit = "d3f449fdfd8cfe4a845d0ae244fce2a0bca34a15",
)

# Expat
git_repository(
    name = "libexpat",
    remote = "https://github.com/libexpat/libexpat.git",
    commit = "fa75b96546c069d17b8f80d91e0f4ef0cde3790d",
    build_file = "libexpat.BUILD",
)

# FDK AAC.
git_repository(
    name = "fdk_aac",
    remote = "https://android.googlesource.com/platform/external/aac",
    commit = "38c27d428ee223bf32f0a2a07cae9fcb99cf3cae",
    build_file = "fdk_aac.BUILD",
)

# FLAC.
git_repository(
    name = "flac",
    remote = "https://github.com/xiph/flac.git",
    commit = "28e4f0528c76b296c561e922ba67d43751990599",
    build_file = "flac.BUILD",
)

# Opus.
git_repository(
    name = "libopus",
    remote = "https://gitlab.xiph.org/xiph/opus.git",
    commit = "82ac57d9f1aaf575800cf17373348e45b7ce6c0d",
    build_file = "libopus.BUILD",
)
