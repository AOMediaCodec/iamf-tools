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
set -uo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
ROOT="${1:-$(pwd)/build}"
SRC="$ROOT/src"
DEPS="$ROOT/deps"
LOG="$ROOT/logs"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
mkdir -p "$SRC" "$DEPS" "$LOG"
git config --global advice.detachedHead false 2>/dev/null || true

stage() { echo "=== $1 [$(date +%H:%M:%S)] ==="; }

clone() { # name url ref mode   (mode: tag = shallow at tag; commit = shallow fetch by sha)
  local name="$1" url="$2" ref="$3" mode="$4" rc=1
  if [ -d "$SRC/$name/.git" ]; then echo "clone $name: exists"; return 0; fi
  case "$mode" in
    tag)
      git clone --depth 1 --branch "$ref" "$url" "$SRC/$name" > "$LOG/clone-$name.log" 2>&1
      rc=$?
      if [ "$rc" -ne 0 ]; then
        # Fallback for hosts where shallow tag clones fail: full clone, then
        # check out the SAME ref — the pin is preserved either way.
        rm -rf "${SRC:?}/${name:?}"
        git clone "$url" "$SRC/$name" >> "$LOG/clone-$name.log" 2>&1 && \
          git -C "$SRC/$name" checkout "$ref" >> "$LOG/clone-$name.log" 2>&1
        rc=$?
      fi
      ;;
    commit)
      git init -q "$SRC/$name" > "$LOG/clone-$name.log" 2>&1 && \
        git -C "$SRC/$name" remote add origin "$url" >> "$LOG/clone-$name.log" 2>&1 && \
        git -C "$SRC/$name" fetch --depth 1 origin "$ref" >> "$LOG/clone-$name.log" 2>&1 && \
        git -C "$SRC/$name" checkout -q FETCH_HEAD >> "$LOG/clone-$name.log" 2>&1
      rc=$?
      ;;
    *)
      echo "clone $name: bad mode '$mode'" >&2
      return 1
      ;;
  esac
  if [ "$rc" -eq 0 ]; then
    echo "clone $name: OK"
  else
    echo "clone $name: FAILED (see $LOG/clone-$name.log)"
    return 1
  fi
}

# ---------- S1: dependency clones (pins mirror MODULE.bazel; see header) ----------
stage "S1 clones"
clone abseil-cpp       https://github.com/abseil/abseil-cpp        20260107.1 tag    || exit 1
clone protobuf         https://github.com/protocolbuffers/protobuf v33.5      tag    || exit 1
clone fdk_aac          https://github.com/mstorsjo/fdk-aac         ee76460efbdb147e26d804c798949c23f174460b commit || exit 1
clone loudness_ebur128 https://github.com/google/loudness_ebur128  e9e73147637db60dc742cba8a611a37dd72b14b5 commit || exit 1
clone obr              https://github.com/google/obr               478dc7c752d5eccae534635139ff0253eee3a14a commit || exit 1
clone audio_to_tactile https://github.com/google/audio-to-tactile  d3f449fdfd8cfe4a845d0ae244fce2a0bca34a15 commit || exit 1
clone pffft            https://github.com/marton78/pffft           a4b03590cc2a4bea56f9721996e3057835799179 commit || exit 1
( cd "$SRC/protobuf" && git submodule update --init --depth 1 third_party/jsoncpp 2>/dev/null; true )

# ---------- S2: header shims for Bazel-style repo-relative includes ----------
stage "S2 shims"
mkdir -p "$SRC/shims/flac/include" "$SRC/shims/opus/include" "$SRC/shims/expat/expat/lib"
ln -sf /usr/include/FLAC "$SRC/shims/flac/include/FLAC"
for h in /usr/include/opus/*.h; do ln -sf "$h" "$SRC/shims/opus/include/"; done
ln -sf /usr/include/expat.h          "$SRC/shims/expat/expat/lib/expat.h"
ln -sf /usr/include/expat_external.h "$SRC/shims/expat/expat/lib/expat_external.h"
ln -sf /usr/include/expat_config.h   "$SRC/shims/expat/expat/lib/expat_config.h" 2>/dev/null || true

# ---------- S3: abseil (static, C++20, propagate std) ----------
stage "S3 abseil"
cmake -S "$SRC/abseil-cpp" -B "$SRC/build-absl" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$DEPS" -DCMAKE_CXX_STANDARD=20 -DABSL_PROPAGATE_CXX_STD=ON \
  -DBUILD_SHARED_LIBS=OFF -DABSL_BUILD_TESTING=OFF > "$LOG/absl-cfg.log" 2>&1 || { echo "ABSL CFG FAIL"; exit 1; }
cmake --build "$SRC/build-absl" -j"$JOBS" --target install > "$LOG/absl-build.log" 2>&1 || { echo "ABSL BUILD FAIL"; exit 1; }

# ---------- S4: protobuf (uses the abseil we just installed; protoc >=27 for edition 2023) ----------
stage "S4 protobuf"
cmake -S "$SRC/protobuf" -B "$SRC/build-protobuf" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$DEPS" -DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_CXX_STANDARD=20 \
  -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_ABSL_PROVIDER=package \
  -Dprotobuf_BUILD_SHARED_LIBS=OFF > "$LOG/pb-cfg.log" 2>&1 || { echo "PB CFG FAIL"; exit 1; }
cmake --build "$SRC/build-protobuf" -j"$JOBS" --target install > "$LOG/pb-build.log" 2>&1 || { echo "PB BUILD FAIL"; exit 1; }

# ---------- S5: iamf-tools via the in-tree CMakeLists ----------
stage "S5 iamf-tools (CMake)"
cmake -S "$REPO" -B "$SRC/build-iamf" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$DEPS" -DIAMF_CMAKE_DEPS_ROOT="$SRC" > "$LOG/port-cfg.log" 2>&1 || { echo "PORT CFG FAIL (see $LOG/port-cfg.log)"; exit 1; }
cmake --build "$SRC/build-iamf" -j"$JOBS" > "$LOG/port-build.log" 2>&1 || { echo "PORT BUILD FAIL (see $LOG/port-build.log)"; exit 1; }

ok=1
for t in encoder_main decoder_main probe_main; do
  if [ -x "$SRC/build-iamf/$t" ]; then echo "built: $SRC/build-iamf/$t"; else echo "MISSING: $t"; ok=0; fi
done
if [ "$ok" = 1 ]; then
  echo "=== DONE: encoder_main / decoder_main / probe_main in $SRC/build-iamf ==="
else
  exit 1
fi
