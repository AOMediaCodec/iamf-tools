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

# FLAC.
git_repository(
    name = "flac",
    build_file = "flac.BUILD",
    commit = "28e4f0528c76b296c561e922ba67d43751990599",
    remote = "https://github.com/xiph/flac.git",
)
