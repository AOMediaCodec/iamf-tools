####################################################################################################
# This project is now manages dependencies with both bazelmod and WORKSPACE, with the goal of moving
# all dependencies to bazelmod.
#
# For more details, please check https://github.com/bazelbuild/bazel/issues/18958.
####################################################################################################

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
)

# Google Audio-to-Tactile Library.
git_repository(
    name = "com_google_audio_to_tactile",
    commit = "d3f449fdfd8cfe4a845d0ae244fce2a0bca34a15",
    remote = "https://github.com/google/audio-to-tactile.git",
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
