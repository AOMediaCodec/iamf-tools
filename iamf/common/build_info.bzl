# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.

"""Rule to generate stamped GetStampedBuildVersion() C++ source from Bazel workspace status."""

def _generate_build_info_impl(ctx):
    args = ctx.actions.args()
    args.add(ctx.version_file)
    args.add(ctx.outputs.out)
    ctx.actions.run_shell(
        inputs = [ctx.version_file],
        outputs = [ctx.outputs.out],
        arguments = [args],
        command = """
build_version=""
while read -r key value; do
  if [ "$key" = "BUILD_CHANGELIST" ] && [ "$value" != "0" ]; then
    build_version="$value"
  fi
done < "$1"
cat > "$2" <<EOF
namespace iamf_tools {
const char* GetStampedBuildVersion() { return "$build_version"; }
}  // namespace iamf_tools
EOF
""",
        mnemonic = "GenerateBuildInfo",
    )

generate_build_info = rule(
    implementation = _generate_build_info_impl,
    attrs = {
        "out": attr.output(mandatory = True),
    },
)
