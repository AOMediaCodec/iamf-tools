# README.md

## Contents

1.  [ADM to IAMF Conversion Tool](#ADM-to-IAMF-Conversion-Tool)
2.  [Building the library](#ADM-Building-the-LibraryTool)
    -   [Prerequisites](#Prerequisites)
    -   [Build the binary](#Build-the-binary)
    -   [Run the binary](#Run-the-binary)
3.  [License](#License)

# ADM to IAMF Conversion Tool

This is a C++ implementation of the tool to convert an ADM-BWF file (conformant
to
[Recommendation ITU-R BS.2076-2](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.2076-2-201910-I!!PDF-E.pdf))
to Immersive Audio Model and Formats (IAMF) textproto file and the associated
set of wav file(s) to be used as inputs for an IAMF encoder. The tool does not
handle the ADM-BWF files with <metadata/features> not supported by the IAMF
specification [v1.0.0-errata](https://aomediacodec.github.io/iamf/v1.0.0-errata.html).

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

1.  [Git](https://git-scm.com/).
2.  [Bazel](https://bazel.build/start).

### Build the binary

```
bazelisk build -c opt app:adm_to_user_metadata_main
```

### Run the binary

Running `bazelisk build` creates the binary file `adm_to_user_metadata_main`. The
input format required to run the binary is as below:

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
-   Audio Element OBUs: audio elements are created based on ADM `audioObjects`.
    Low importance objects are filtered out based on the
    `--importance_threshold` flag.
-   Mix Presentation OBUs: mix presentations are generated based on ADM
    `audioProgramme`s.

## License

Released under the BSD 3-Clause Clear License. See [LICENSE](LICENSE) for
details.
