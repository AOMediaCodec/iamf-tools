#!/usr/bin/env bash
# build_cmake.sh — optional CMake build path for encoder_main / decoder_main /
# probe_main, for environments where Bazel is unavailable (registry-restricted
# networks: git clone allowed; Bazel / BCR / release archives blocked).
# See iamf-tools#75. Bazel remains the supported build system of record.
#
# Written from scratch against the Bazel BUILD graph; no Bazel-generated
# content is used or derived.
#
# Dependency pins mirror MODULE.bazel exactly, with two stated exceptions:
#   - pffft: MODULE.bazel pins jpommier's bitbucket repo (d7a4c020); this path
#     uses the marton78 GitHub mirror at a fixed commit (a4b03590), because
#     restricted environments that motivate this build path typically allow
#     github.com only. The mirror keeps sources under src/.
#   - opus / flac / expat / eigen: satisfied from system packages rather than
#     source pins (header shims below provide the Bazel-style include paths).
# Move the pins forward as MODULE.bazel moves.
#
# Usage:
#   ./build_cmake.sh [ROOT]   # ROOT defaults to ./build (gitignored is fine;
#                             # nothing under iamf/ is written)
# Requirements (Debian/Ubuntu names): cmake pkg-config git g++
#   libopus-dev libflac-dev libexpat1-dev libeigen3-dev
# Requirements (macOS / Homebrew):     brew install cmake pkg-config opus flac eigen
#   expat is supplied by the Xcode Command Line Tools SDK. macOS has no
#   /usr/include at all, so system headers are discovered (S2 below) and eigen
#   is located with find_path rather than a fixed prefix.
set -uo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
ROOT="${1:-$(pwd)/build}"
SRC="$ROOT/src"
DEPS="$ROOT/deps"
LOG="$ROOT/logs"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
mkdir -p "$SRC" "$DEPS" "$LOG"

stage() { echo "=== $1 [$(date +%H:%M:%S)] ==="; }

run_logged() {
  local log_name="$1"; shift
  if ! "$@" > "$LOG/$log_name.log" 2>&1; then
    echo "$log_name: FAILED (see $LOG/$log_name.log)" >&2
    exit 1
  fi
}

clone() { # name url ref
  local name="$1" url="$2" ref="$3"
  if [ -d "$SRC/$name/.git" ]; then echo "clone $name: exists"; return 0; fi
  if git init -q "$SRC/$name" > "$LOG/clone-$name.log" 2>&1 && \
     git -C "$SRC/$name" fetch --depth 1 "$url" "$ref" >> "$LOG/clone-$name.log" 2>&1 && \
     git -C "$SRC/$name" -c advice.detachedHead=false checkout -q FETCH_HEAD >> "$LOG/clone-$name.log" 2>&1; then
    echo "clone $name: OK"
  else
    echo "clone $name: FAILED (see $LOG/clone-$name.log)"
    return 1
  fi
}

cmake_install() {
  local name="$1" src_dir="$2" build_dir="$3"; shift 3
  stage "Building $name"
  run_logged "$name-cfg" cmake -S "$src_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$DEPS" \
    -DCMAKE_PREFIX_PATH="$DEPS" \
    -DCMAKE_CXX_STANDARD=20 \
    "$@"
  run_logged "$name-build" cmake --build "$build_dir" -j"$JOBS" --target install
}

# ---------- S1: dependency clones (pins mirror MODULE.bazel; see header) ----------
stage "S1 clones"
clone abseil-cpp       https://github.com/abseil/abseil-cpp        20260107.1 || exit 1
clone protobuf         https://github.com/protocolbuffers/protobuf v33.5      || exit 1
clone fdk_aac          https://github.com/mstorsjo/fdk-aac         ee76460efbdb147e26d804c798949c23f174460b || exit 1
clone loudness_ebur128 https://github.com/google/loudness_ebur128  e9e73147637db60dc742cba8a611a37dd72b14b5 || exit 1
clone obr              https://github.com/google/obr               478dc7c752d5eccae534635139ff0253eee3a14a || exit 1
clone audio_to_tactile https://github.com/google/audio-to-tactile  d3f449fdfd8cfe4a845d0ae244fce2a0bca34a15 || exit 1
clone pffft            https://github.com/marton78/pffft           a4b03590cc2a4bea56f9721996e3057835799179 || exit 1

# ---------- S2: header shims for Bazel-style repo-relative includes ----------
stage "S2 shims"
# System header prefixes are not portable: Debian/Ubuntu use /usr/include,
# Homebrew uses $(brew --prefix)/include, and macOS has no /usr/include at all
# (expat ships inside the Xcode Command Line Tools SDK). Discover each header
# instead of hardcoding one distro's layout.
HDR_PREFIXES="/usr/include"
_brew="$(brew --prefix 2>/dev/null || true)"
[ -n "$_brew" ] && HDR_PREFIXES="$HDR_PREFIXES $_brew/include"
_sdk="$(xcrun --show-sdk-path 2>/dev/null || true)"
[ -n "$_sdk" ] && HDR_PREFIXES="$HDR_PREFIXES $_sdk/usr/include"
find_hdr() { for p in $HDR_PREFIXES; do [ -e "$p/$1" ] && { printf '%s\n' "$p/$1"; return 0; }; done; return 1; }

mkdir -p "$SRC/shims/flac/include" "$SRC/shims/opus/include" "$SRC/shims/expat/expat/lib"
FLAC_H="$(find_hdr FLAC)"     || { echo "S2 FAIL: FLAC headers not found in: $HDR_PREFIXES" >&2; exit 1; }
OPUS_H="$(find_hdr opus)"     || { echo "S2 FAIL: opus headers not found in: $HDR_PREFIXES" >&2; exit 1; }
EXPAT_H="$(find_hdr expat.h)" || { echo "S2 FAIL: expat.h not found in: $HDR_PREFIXES" >&2; exit 1; }
ln -sf "$FLAC_H" "$SRC/shims/flac/include/FLAC"
for h in "$OPUS_H"/*.h; do ln -sf "$h" "$SRC/shims/opus/include/"; done
ln -sf "$EXPAT_H" "$SRC/shims/expat/expat/lib/expat.h"
for extra in expat_external.h expat_config.h; do
  _e="$(find_hdr "$extra")" && ln -sf "$_e" "$SRC/shims/expat/expat/lib/$extra"
done
# `ln -sf` to a NONEXISTENT target succeeds and returns 0, so without this check
# the shim stage cannot fail on a missing dependency -- the error instead surfaces
# hundreds of lines later as a confusing missing-header compile failure.
for l in "$SRC/shims/flac/include/FLAC" "$SRC/shims/expat/expat/lib/expat.h"; do
  [ -e "$l" ] || { echo "S2 FAIL: dangling shim symlink $l -> $(readlink "$l")" >&2; exit 1; }
done
echo "S2 shims OK: FLAC=$FLAC_H opus=$OPUS_H expat=$EXPAT_H"

# ---------- S3: abseil (static, C++20, propagate std) ----------
cmake_install abseil "$SRC/abseil-cpp" "$SRC/build-absl" \
  -DABSL_PROPAGATE_CXX_STD=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DABSL_BUILD_TESTING=OFF

# ---------- S4: protobuf (depends on abseil above) ----------
cmake_install protobuf "$SRC/protobuf" "$SRC/build-protobuf" \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_ABSL_PROVIDER=package \
  -Dprotobuf_BUILD_SHARED_LIBS=OFF

# ---------- S5: iamf-tools via the in-tree CMakeLists ----------
stage "Building iamf-tools"
run_logged "iamf-cfg" cmake -S "$REPO" -B "$SRC/build-iamf" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$DEPS" \
  -DIAMF_CMAKE_DEPS_ROOT="$SRC"
run_logged "iamf-build" cmake --build "$SRC/build-iamf" -j"$JOBS"

ok=1
for t in encoder_main decoder_main probe_main; do
  if [ -x "$SRC/build-iamf/$t" ]; then echo "built: $SRC/build-iamf/$t"; else echo "MISSING: $t"; ok=0; fi
done
if [ "$ok" = 1 ]; then
  echo "=== DONE: encoder_main / decoder_main / probe_main in $SRC/build-iamf ==="
else
  exit 1
fi
