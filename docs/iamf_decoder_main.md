## Decoder

`iamf-tools` provides an example command-line decoder to process a standalone
IAMF sequence (`.iamf`) file to a multichannel WAV file.

### Building the decoder

See [Build instructions](build_instructions.md) to build from source.

### Using the decoder

The `iamf-tools` command-line decoder can decode an IAMF file to a multichannel
WAV file using the following flags:

Required flags:

-   `--input_filename` Path to the input `.iamf` file.
-   `--output_filename` Path to write the output WAV file.

Optional flags:

-   `--mix_id` Mix Presentation to render (default is automatic selection).
-   `--output_layout` Output layout to render to (default is `2.0`).

Example:

```
bazel-bin/iamf/cli/decoder_main --input_filename=iamf/cli/testdata/iamf/tones_256samp_5p1_pcm.iamf --output_filename=output_file.wav
```

If successful, the decoder will produce an `output_file.wav` file in the current
directory.
