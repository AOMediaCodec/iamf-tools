# iamf-tools

## What is IAMF?

The [Immersive Audio Model and Formats](https://aomediacodec.github.io/iamf/)
(IAMF) standard is defined by the Alliance for Open Media (AOM).

## What is `iamf-tools`?

`iamf-tools` implements tools to help users process and work with the IAMF
format. These tools can be used as a complement to the `libiamf`
[reference decoder](https://github.com/AOMediaCodec/libiamf/) and other tools
such as [ffmpeg](https://ffmpeg.org/) and
[MP4Box](https://github.com/gpac/gpac?tab=readme-ov-file#mp4box).

### Encoding IAMF content

There are multiple workflows for creating IAMF content, supporting different
types of input or output.

![IAMF encode workflows](docs/iamf-encode-workflows.svg "IAMF encode workflows")

`iamf-tools` provides a few ways to encode IAMF content.

-   [`IamfEncoderInterface` API](docs/iamf_encoder_interface.md)
-   [Command-line encoder for WAV or ADM-BWF files](docs/iamf_encoder_main.md)

Additionally, documentation is provided for third-party workflows. In particular
these are useful for working with IAMF in MP4, and IAMF muxed with video.

-   [Merge IAMF files and video into an an MP4 file](docs/external/encoding_with_external_tools.md#merge-an-iamf-file-with-video-into-mp4)
-   [Encode WAV files to IAMF using ffmpeg](docs/external/encoding_with_external_tools.md#encode-wav-files-to-iamf-with-ffmpeg)

### Decoding IAMF content

The `libiamf` reference decoder and its documentation are available at
https://github.com/AOMediaCodec/libiamf/. It provides decoding tools for
standalone IAMF and IAMF in MP4.

`iamf-tools` provides a different implementation and command-line decoder:

-   [`IamfDecoderInterface` API](docs/iamf_decoder_interface.md)
-   [Command-line decoder for standalone IAMF files](docs/iamf_decoder_main.md)

### Web Demo

The [web demo](https://aomediacodec.github.io/iamf-tools/web_demo/) hosted in
the GitHub Pages of this repo decodes and renders standalone IAMF files to
stereo and binaural WAV files for preview and comparison.

## License

Released under the BSD 3-Clause Clear License. See [LICENSE](LICENSE) for
details.
