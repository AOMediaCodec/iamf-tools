# Build instructions

## Prerequisites for building the encoder

### All platforms

-   Bazel: `iamf-tools` uses the Bazel build system. See Bazel's
    [Getting started](https://bazel.build/start) page for installation
    instructions.
-   CMake: required to build some dependencies. See CMake's
    [Download](https://cmake.org/download/) page to install.
-   Clang 13+ or GCC 10+ for Linux-like systems or MSVC for Windows.

### Additional Windows prerequisites

Windows requires some additional prerequisites:

-   MSYS32: See [msys2.org](https://www.msys2.org/) for installation
    instructions.
-   MSCV: Follow Bazel's
    [Build on Windows](https://bazel.build/configure/windows) for installation
    instructions.
-   Address
    [long path issues](https://bazel.build/configure/windows#long-path-issues)
    by adding the following to `iamf-tools/.bazelrc` or another
    [custom .bazelrc](https://bazel.build/run/bazelrc) file:

    `startup --output_user_root=C:/tmp`

## Building the encoder

Building the encoder binary:

```
bazel build -c opt //iamf/cli:encoder_main
```

Running built-in tests:

```
bazel test -c opt //iamf/...
```

## Test suite

[iamf/cli/testdata](../iamf/cli/testdata) covers a wide variety of IAMF
features. These samples can be used as encoder input. After encoding the
resultant `.iamf` files can be used to assist in testing or debugging an
IAMF-compliant decoder.

See the separate [README.md](../iamf/cli/testdata/README.md) for further
documentation.

## Folder structure

*   `<root>` - Project-level files like the license, README (this file), and
    BUILD files.
*   `iamf/`
    *   `common/` - Common utility files.
        *   `tests/` - Unit tests for files under `common/`.
    *   `cli/` - Files related to the command line interface (CLI) to generate
        and write an IA Sequence.
        *   `adm_to_user_metadata/` - Components to convert ADM files to
            protocol buffers which can be used for input to the encoder.
        *   `codec/` - Files that encode or decode substreams with
            codec-specific libraries.
            *   `tests/` - Unit tests for files under `codec/`.
        *   `obu_to_proto/` - Components to convert classes under `iamf/obu` to
            protocol buffers.
        *   `proto/` - [Protocol buffers](https://protobuf.dev/) defining the
            data scheme for the input files under `iamf/cli/testdata/` or
            user-created ones.
        *   `proto_to_obu/` - Components to convert protocol buffers to classes
            under `iamf/obu`.
        *   `testdata/` - Sample input files to the encoder. See also
            [Test Suite](#Test-Suite).
        *   `tests/` - Unit tests for files under `iamf/cli/`.
        *   `user_metadata_builder/` - Files to help build a `UserMetadata`.
    *   `obu/` - Files relating to IAMF Open Bitstream Units (OBU)s.
        *   `decoder_config/` - Files relating codec-specific decoder configs.
            *   `tests/` - Unit tests for files under `decoder_config/`.
        *   `tests/` - Unit tests for files under `obu/`.
*   `external/` - Custom `*.BUILD` files for external dependencies.
*   `docs/` - Documentation.

## Contributing

If you have improvements or fixes, we would love to have your contributions. See
[CONTRIBUTING.md](../CONTRIBUTING.md) for details.
