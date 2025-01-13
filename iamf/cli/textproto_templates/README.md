# Textproto templates

The textproto templates correspond to the following audio layouts.

| Input layout                  | Codec | File name                         |
| ----------------------------- | ----- | --------------------------------- |
| Stereo                        | PCM   | stereo_pcm24bit.textproto         |
| Stereo                        | Opus  | stereo_opus.textproto             |
| 5.1                           | PCM   | 5dot1_pcm24bit.textproto          |
| 5.1                           | Opus  | 5dot1_opus.textproto              |
| 5.1.2                         | PCM   | 5dot1dot2_pcm24bit.textproto      |
| 5.1.2                         | Opus  | 5dot1dot2_opus.textproto          |
| 7.1.4                         | PCM   | 7dot1dot4_pcm24bit.textproto      |
| 7.1.4                         | Opus  | 7dot1dot4_opus.textproto          |
| 1st order Ambisonics          | PCM   | 1OA_pcm24bit.textproto            |
| 1st order Ambisonics          | Opus  | 1OA_opus.textproto                |
| 3rd order Ambisonics          | PCM   | 3OA_pcm24bit.textproto            |
| 3rd order Ambisonics          | Opus  | 3OA_opus.textproto                |
| 1st order Ambisonics + Stereo | PCM   | 1OA_and_stereo_pcm24bit.textproto |
| 1st order Ambisonics + Stereo | Opus  | 1OA_and_stereo_opus.textproto     |
| 3rd order Ambisonics + Stereo | PCM   | 3OA_and_stereo_pcm24bit.textproto |
| 3rd order Ambisonics + Stereo | Opus  | 3OA_and_stereo_opus.textproto     |

## Customizing the templates

Set the following fields in the textproto template.

- `wav_filename`

    Set this to the input wav filename, without the path to it. For example, if
    the wav file is located at `/path/to/input.wav`, set

    ```
    wav_filename: input.wav
    ```

- `file_name_prefix`

    Set this to the desired output filename. The generated .iamf file will be
    named `file_name_prefix.iamf`.

- `loudness`

    Measure the loudness of the input audio, including the stereo downmix, and
    store these values in the following loudness fields. IAMF decoders and
    renderers can use this loudness metadata to normalize the output audio.

    - `loudness.integrated_loudness`

        This is the [ITU-R BS.1770-4](https://www.itu.int/rec/R-REC-BS.1770)
        integrated loudness, specified in LKFS. Convert the loudness value to
        the correct `int16` value to use here as `integrated_loudness =
        integrated_loudness_in_lkfs * 256`.

    - `loudness.digital_peak`

        This is the digital (sampled) peak value of the audio signal, specified
        in dBFS. Convert the peak value to the correct `int16` value to use here
        as `digital_peak = digital_peak_in_dBFS * 256`.

Optionally, modify other fields in the textproto template as necessary.

- `channel_metadatas`

    Use custom `channel_ids` when the input wav file is in a different order. By
    default they are configured for wav files using
    [ITU-2051-3](https://www.itu.int/rec/R-REC-BS.2051) order.

- `headphones_rendering_mode`

    Choose one of `HEADPHONES_RENDERING_MODE_BINAURAL` or `HEADPHONES_RENDERING_MODE_STEREO`.

    This informs the renderer if the audio element should be binauralized or downmixed to stereo.

- `element_mix_gain.default_mix_gain`

    This is the gain that will be applied to an audio element before it is summed with all other audio elements.

    It is denoted in dB and Q7.8 format, and then converted to `int16`. Convert a desired gain to the correct `int16` value to use here as `default_mix_gain = gain_in_db * 256`.

- `output_mix_gain.default_mix_gain`

    This is the gain that will be applied to the summed audio elements.

    It is denoted in dB and Q7.8 format, and then converted to `int16`. Convert a desired gain to the correct `int16` value to use here as `default_mix_gain = gain_in_db * 256`.


The following are available for PCM textprotos only.

- `decoder_config_lpcm.sample_size`

    Input PCM bit-depth. Allowed values are 16, 24, 32.

- `decoder_config_lpcm.sample_rate`

    Input and output sample rate. Allowed values are 16000, 32000, 44100, 48000, 96000.

    All `parameter_rate` values must additionally be updated to match `sample_rate`.


The following are available for Opus textprotos only.

- `opus_encoder_metadata.target_bitrate_per_channel`

    Target bitrate per channel, in bits per second.

- `opus_encoder_metadata.coupling_rate_adjustment`

    Some channels are coupled and coded as stereo pairs, e.g., L/R. This field adjusts the total target bitrate of the coupled channels to be `target_bitrate_per_channel * 2 * coupling_rate_adjustment`. Its default value is 1.0.
