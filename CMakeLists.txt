# Optional CMake build path for the encoder_main / decoder_main / probe_main
# CLIs, for environments where Bazel is unavailable (registry-restricted
# networks, no BCR / release-archive access). See iamf-tools#75.
#
# This is NOT the supported build; Bazel remains the build system of record
# (see docs/build_instructions.md). This file is written from scratch against
# the Bazel BUILD graph; no Bazel-generated content is used or derived.
#
# Invoke via ./build_cmake.sh (which clones and builds the dependencies, then
# configures this file with IAMF_CMAKE_DEPS_ROOT pointing at those checkouts).
# Dependency pins mirror MODULE.bazel; move them forward as upstream moves.
cmake_minimum_required(VERSION 3.22)
project(iamf_tools LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wno-sign-compare)

set(IAMF ${CMAKE_CURRENT_SOURCE_DIR})
set(IAMF_CMAKE_DEPS_ROOT "" CACHE PATH
    "Directory holding the dependency checkouts and shims (populated by build_cmake.sh)")
if(NOT IAMF_CMAKE_DEPS_ROOT OR NOT EXISTS ${IAMF_CMAKE_DEPS_ROOT})
  message(FATAL_ERROR
      "IAMF_CMAKE_DEPS_ROOT is not set (or does not exist). This build path is "
      "driven by ./build_cmake.sh, which clones the dependencies and passes "
      "-DIAMF_CMAKE_DEPS_ROOT=<deps dir>. Run ./build_cmake.sh [ROOT] instead "
      "of invoking cmake directly.")
endif()
set(SRC ${IAMF_CMAKE_DEPS_ROOT})

find_package(absl REQUIRED CONFIG)
find_package(utf8_range CONFIG)          # exported by protobuf install
find_package(protobuf REQUIRED CONFIG)

find_library(OPUS_LIB opus REQUIRED)
find_library(FLAC_LIB FLAC REQUIRED)
find_library(EXPAT_LIB expat REQUIRED)

# Eigen is header-only and ships under an `eigen3/` suffix on every distro, but
# the prefix is not portable: Debian/Ubuntu use /usr/include/eigen3, Homebrew
# uses ${brew_prefix}/include/eigen3, and macOS has no /usr/include at all.
# Discover it rather than hardcoding one distro's layout.
find_path(EIGEN_INC NAMES Eigen/Core PATH_SUFFIXES eigen3 REQUIRED)

# ---------------- FDK-AAC (exact upstream pin, own CMake) -------------------
# Non-recursive on purpose: lib*/src/{arm,mips,x86}/ hold #include-only
# assembly specializations that do not compile standalone.
file(GLOB FDK_SRCS ${SRC}/fdk_aac/lib*/src/*.cpp)
add_library(fdk_aac_static STATIC ${FDK_SRCS})
file(GLOB FDK_INC_DIRS ${SRC}/fdk_aac/lib*/include)
target_include_directories(fdk_aac_static PUBLIC ${FDK_INC_DIRS} ${SRC}/fdk_aac)

# ---------------- pffft ------------------------------------------------------
# marton78/pffft mirror keeps sources under src/ (upstream jpommier keeps them
# at repo root); pffft_common.c provides the shared aligned-malloc helpers.
add_library(pffft_static STATIC ${SRC}/pffft/src/pffft.c ${SRC}/pffft/src/pffft_common.c)
target_include_directories(pffft_static PUBLIC
    ${SRC}/pffft/src ${SRC}/pffft/include/pffft)

# ---------------- audio-to-tactile :dsp -------------------------------------
file(GLOB A2T_SRCS ${SRC}/audio_to_tactile/src/dsp/*.c)
add_library(a2t_dsp STATIC ${A2T_SRCS})
target_include_directories(a2t_dsp PUBLIC ${SRC}/audio_to_tactile/src ${SRC}/audio_to_tactile)
target_link_libraries(a2t_dsp PUBLIC m)

# ---------------- loudness_ebur128 ------------------------------------------
file(GLOB EBUR_SRCS ${SRC}/loudness_ebur128/src/*.cc)
add_library(ebur128_analyzer STATIC ${EBUR_SRCS})
target_include_directories(ebur128_analyzer PUBLIC
    ${SRC}/loudness_ebur128 ${SRC}/loudness_ebur128/src)
target_link_libraries(ebur128_analyzer PUBLIC absl::status absl::statusor
    absl::strings absl::span)

# ---------------- OBR (binaural renderer) -----------------------------------
file(GLOB_RECURSE OBR_SRCS ${SRC}/obr/obr/*.cc)
list(FILTER OBR_SRCS EXCLUDE REGEX "/tests?/|_test\\.cc|test_util\\.cc|/cli/")
add_library(obr_lib STATIC ${OBR_SRCS})
target_include_directories(obr_lib PUBLIC ${SRC}/obr ${EIGEN_INC})
target_link_libraries(obr_lib PUBLIC pffft_static a2t_dsp
    absl::log absl::check absl::status absl::statusor absl::strings
    absl::flat_hash_map absl::span)

# ---------------- proto codegen ---------------------------------------------
file(GLOB IAMF_PROTOS ${IAMF}/iamf/cli/proto/*.proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
set(PROTO_SRCS "")
foreach(P ${IAMF_PROTOS})
  file(RELATIVE_PATH REL ${IAMF} ${P})
  string(REPLACE ".proto" ".pb.cc" GEN_CC ${PROTO_GEN_DIR}/${REL})
  string(REPLACE ".proto" ".pb.h"  GEN_H  ${PROTO_GEN_DIR}/${REL})
  add_custom_command(
    OUTPUT ${GEN_CC} ${GEN_H}
    COMMAND protobuf::protoc --proto_path=${IAMF} --cpp_out=${PROTO_GEN_DIR} ${REL}
    DEPENDS ${P} protobuf::protoc
    WORKING_DIRECTORY ${IAMF})
  list(APPEND PROTO_SRCS ${GEN_CC})
endforeach()
add_library(iamf_protos STATIC ${PROTO_SRCS})
target_include_directories(iamf_protos PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(iamf_protos PUBLIC protobuf::libprotobuf)

# ---------------- iamf-tools core -------------------------------------------
file(GLOB_RECURSE IAMF_SRCS ${IAMF}/iamf/*.cc)
list(FILTER IAMF_SRCS EXCLUDE REGEX "/tests/|_test\\.cc|_fuzz|_benchmark")
list(FILTER IAMF_SRCS EXCLUDE REGEX "encoder_main\\.cc|decoder_main\\.cc|probe_main\\.cc|adm_to_user_metadata_main\\.cc")
add_library(iamf_core STATIC ${IAMF_SRCS})
target_include_directories(iamf_core PUBLIC
    ${IAMF}
    ${SRC}/protobuf              # bazel-style "src/google/protobuf/..." includes
    ${SRC}/shims/flac            # "include/FLAC/..."
    ${SRC}/shims/opus            # "include/opus.h"
    ${SRC}/shims/expat           # "expat/lib/expat.h"
    ${EIGEN_INC})
target_link_libraries(iamf_core PUBLIC
    iamf_protos ebur128_analyzer obr_lib a2t_dsp fdk_aac_static
    ${OPUS_LIB} ${FLAC_LIB} ${EXPAT_LIB}
    absl::base absl::log absl::log_initialize absl::log_globals absl::check
    absl::flags absl::flags_parse absl::flags_usage
    absl::status absl::statusor absl::strings absl::str_format
    absl::flat_hash_map absl::flat_hash_set absl::node_hash_map
    absl::span absl::optional absl::time absl::synchronization
    absl::memory absl::random_random absl::die_if_null absl::cleanup
    absl::no_destructor absl::overload absl::any_invocable)

foreach(TOOL encoder_main decoder_main probe_main)
  add_executable(${TOOL} ${IAMF}/iamf/cli/${TOOL}.cc)
  target_link_libraries(${TOOL} PRIVATE iamf_core)
endforeach()
