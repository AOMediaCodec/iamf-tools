#!/bin/bash

# See Bazel's workspace status command
# (https://bazel.build/docs/user-manual#workspace-status).

version="$(git describe --tags --long --always --dirty 2>/dev/null)"
echo "Stamping build info: BUILD_CHANGELIST=${version:-unknown}" >&2
echo "BUILD_CHANGELIST ${version}"
