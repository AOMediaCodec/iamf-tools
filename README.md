# iamf-tools

## What is IAMF?

The [Immersive Audio Model and Formats](https://aomediacodec.github.io/iamf/)
(IAMF) standard is defined by the Alliance for Open Media (AOM).

## What is `iamf-tools`?

`iamf-tools` implements tools to help users process and work with the IAMF
format. These tools can be used as a complement to the `libiamf`
[reference decoder](https://github.com/AOMediaCodec/libiamf/).


### Folder Structure
* `<root>` - Project-level files like the license, README (this file), and BUILD files.
* `iamf/` - Common data classes for the Open Bitstream Units (OBU)s and tools.
    * `cli/` - Files related to the command line interface (CLI) to generate and write an IA Sequence.
        * `proto/` - [Protocol buffers](https://protobuf.dev/) defining the data scheme for the input files under `iamf/cli/testdata/` or user-created ones.
        * `testdata/` - Sample input files to the encoder. See also [Test Suite](#Test-Suite).
        * `tests/` - Unit tests for files under `iamf/cli/`.
  * `tests/` - Unit tests for files under `iamf/`.
* `external/` - Custom `*.BUILD` files for external dependencies.

### Encoder

The encoder can be used to encode a standalone IAMF Sequence (`.iamf`) file
based on an input `.textproto` file. See [Test Suite](#Test-Suite) for several
example input files.

#### Prerequisites for building the encoder

`iamf-tools` is built using Bazel. See Bazel's
[Getting started](https://bazel.build/start) page for installation instructions.

#### Building the encoder on Linux

Building the encoder binary:

```
bazel build -c opt //iamf/cli:encoder_main
```

Running built-in tests:

```
bazel test //iamf/...
```

#### Using the encoder

Run the encoder. Specify the input file with `--user_metadata_filename` and the
output directory with `--output_iamf_directory`.

Using the encoder:

```
bazel-bin/iamf/cli/encoder_main --user_metadata_filename=iamf/cli/testdata/test_000002.textproto --output_iamf_directory=.
```

If this example is successful the encoder will produce an output
`test_000002.iamf` file in the current directory.

### Test suite

[iamf/cli/testdata](iamf/cli/testdata) cover a wide variety of IAMF feature. These samples
can be used as encoder input. After encoding the resultant `.iamf` files can be
used to assist in testing or debugging an IAMF-compliant decoder.

See the separate [REAMDE.md](iamf/cli/testdata/README.md) for further documentation.

## License

Released under the BSD 3-Clause Clear License. See [LICENSE](LICENSE) for
details.
