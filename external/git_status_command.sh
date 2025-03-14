#!/bin/bash

# A script designed to work with bazel's workspace status command
# (https://bazel.build/docs/user-manual#workspace-status). This script prints
# output in key-value pairs.
#   - {BUILD_CHANGELIST, git commit hash or "unknown" if it fails}

echo -n "BUILD_CHANGELIST "
echo "$(git rev-parse HEAD)" || echo "unknown"
