# README.md

## Contents

1.  [ADM to IAMF Conversion Tool](#ADM-to-IAMF-Conversion-Tool)
2.  [Building the library](#ADM-Building-the-LibraryTool)
    -   [Prerequisites](#Prerequisites)
    -   [Build the binary](#Build-the-binary)
    -   [Run the binary](#Run-the-binary)
3.  [License](#License)

# ADM to IAMF Conversion Tool

This is a C++ implementation of a tool to extract metadata and audio from an
ADM-BWF file (conformant to
[Recommendation ITU-R BS.2076-2](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.2076-2-201910-I!!PDF-E.pdf)).

Information is extracted and written to a `UserMetadata` proto file and a set of
wav file(s). These files can be consumed by the `iamf-tools` encoder to produce
an Immersive Audio Model and Formats (IAMF) file.

Most users do not need to use this tool directly. ADM conversion is available
through various command line flags in
[encoder_main](../../../README.md##using-the-encoder-with-adm-bwf-input).

## Folder Structure

The description of each directory is as follows:

*   `adm` - Components related to parsing an ADM-BWF file.
    *   `tests` - Tests for files under `adm`.
*   `app` - An example application which uses the conversion tool.
    *   `tests` - Tests for files under `app`.
*   `iamf` - Components related to transforming ADM into user metadata suitable
    for the `iamf-tools` encoder.
    *   `tests` - Tests for files under `iamf`.

## Building the library

### Prerequisites

See [docs/build_instructions.md](../../../docs/build_instructions.md) for
further details.

-   Bazelisk: `iamf-tools` uses the Bazel build system, via bazelisk. See
    [Bazelisk installation instructions](https://bazel.build/install/bazelisk).
    For further information on Bazel, see
    [Getting started](https://bazel.build/start).
-   CMake: required to build some dependencies. See CMake's
    [Download](https://cmake.org/download/) page to install.
-   Clang 13+ or GCC 10+ for Linux-like systems or MSVC for Windows.

### Build the binary

```
bazelisk build -c opt //iamf/cli/adm_to_user_metadata/app:adm_to_user_metadata_main
```

### Run the binary

Running `bazelisk build` creates the binary file `adm_to_user_metadata_main`.
The input format required to run the binary is as below:

```
 ./adm_to_user_metadata_main <options>

options:
  --adm_filename (required) - ADM wav file to process.
  --importance_threshold (optional) - Used to reject objects whose importance is below the given threshold of range 0 to 10 (default 0).
  --frame_duration_ms (optional) - Target frame duration in ms. The actual output may be slightly lower due to rounding. (default 10).
  --write_binary_proto (optional) - Whether to write the output as a binary proto or a textproto (default true)
  --output_file_path (optional) - Path to write output files to. (default current directory)


  Example:
    - ./adm_to_user_metadata_main --adm_filename input.wav
    - ./adm_to_user_metadata_main --write_binary_proto=false --adm_filename input.wav
    - ./adm_to_user_metadata_main --frame_duration_ms=20 --adm_filename input.wav
    - ./adm_to_user_metadata_main --importance_threshold=4 --adm_filename input.wav
```

### Conversion details

-   IA Sequence Header OBU: a base profile sequence header is always generated.
-   Codec Config OBU: an LPCM codec config is created with sample rate and
    bit-depth determined based on the input file. `number_of_samples_per_frame`
    is determined based on the `--frame_duration_ms` flag.
-   Audio Element OBUs:
    -   When audio elements are a type directly representable in IAMF, they are
        created based on ADM `audioObjects`.
    -   When audio elements aren't directly representable in IAMF, they may be
        folded to third-order ambisonics. See the "Warning" at the end of this
        section.
    -   Some types directly representable in IAMF include {stereo, 5.1, 7.1.4,
        third_order_ambisonics}. One type that is not representable directly in
        IAMF includes objects.
    -   Low importance objects are filtered out based on the
        `--importance_threshold` flag.
-   Mix Presentation OBUs: mix presentations are generated based on ADM
    `audioProgramme`s.

> [!WARNING]
>
> Some ADM conversions are a work in progress and are experimental
> (b/392958726).

## License

Released under the BSD 3-Clause Clear License. See [LICENSE](LICENSE) for
details.
