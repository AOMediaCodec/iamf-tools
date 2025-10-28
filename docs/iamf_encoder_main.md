## Encoder

`iamf-tools` provides an example command-line encoder to produce a standalone
IAMF sequence (`.iamf`) file from input audio files.

### Building the encoder

See [Build instructions](build_instructions.md) to build from source.

### Using the encoder with WAV files and proto input

The `iamf-tools` command-line encoder can encode a set of multichannel WAV files
into an IAMF file. An accompanying input textproto file configures channel
layout, mix gains, and codecs for the audio elements.

See [textproto templates](../iamf/cli/textproto_templates) common
configurations. See the [test suite textprotos](../iamf/cli/testdata) for
details on further customization and advanced features.

Flags for WAV file input:

-   `--user_metadata_filename` Required path to the input textproto
    configuration file.
-   `--input_wav_directory` Directory to read WAV files from (default:
    `iamf/cli/testdata/`).
-   `--output_iamf_directory` Directory to write IAMF files to (default: `.`).

Example:

```
bazel-bin/iamf/cli/encoder_main --user_metadata_filename=iamf/cli/testdata/test_000002.textproto --output_iamf_directory=.
```

If successful, the encoder will produce an output `test_000002.iamf` file in the
current directory.

### Using the encoder with ADM-BWF input

The encoder can also accept ADM-BWF (`.wav`) files as input using the
`--adm_filename` flag. See the `adm_to_user_metadata`
[README](/iamf/cli/adm_to_user_metadata/README.md) for details on the
conversion.

> [!WARNING]
>
> Some ADM conversions are experimental and a work in progress (b/392958726).

Flags for ADM-BWF input:

-   `--adm_filename` Required path to input ADM-BWF WAV file.
-   `--output_iamf_directory` Directory to write IAMF files to (default: `.`).
-   `--adm_importance_threshold` Threshold below which ADM `audioObject`s are
    omitted (default: 0).
-   `--adm_frame_duration_ms` Frame size of the output IAMF in milliseconds
    (default: 10).

Example:

```
bazel-bin/iamf/cli/encoder_main --adm_filename=path/to/adm.wav --output_iamf_directory=.
```

The encoder will produce an output `.iamf` file in the output directory, where
the underlying audio streams are encoded with PCM.
