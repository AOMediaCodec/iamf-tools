# Encoding with external tools

The [FFmpeg](https://www.ffmpeg.org/) CLI can be used to encode a set of input
wav files to IAMF and merge with a video file into an MP4 file. Some example
commands are listed below. See the [official ffmpeg
documentation](https://www.ffmpeg.org/ffmpeg.html) for more details.

## Encode IAMF audio and merge with video using ffmpeg

The examples below encode input wav files to IAMF using Opus to encode the
underlying audio elements at 64 kbps per channel.

Usage notes:

- **Input channel order**

    The examples assume that the channel order in the input wav files follow the
    ordering used in [ITU-R BS.2051](https://www.itu.int/rec/R-REC-BS.2051).

    If a different channel order is used, change the input channel indices
    indicated by `channelmap`. The [IAMF specification (Coupled stereo
    channels)](https://aomediacodec.github.io/iamf/v1.1.0.html#coupled_substream_count)
    requires that the result groups specific channels as either a stereo pair or
    a mono channel, and additionally that they follow a specific order. In the
    ffmpeg command, this is defined by the order of the `-map "[]"` options.

- **Loudness metadata**

    IAMF files include loudness metadata for the input audio, which IAMF
    decoders and renderers can use to normalize the output audio.

    The `integrated_loudness` value specifies the program integrated loudness
    information in LKFS, as measured according to
    [ITU-R BS.1770-4](https://www.itu.int/rec/R-REC-BS.1770).

    The `digital_peak` value specifies the digital (sampled) peak value of the
    audio signal in dBFS.

    To include the correct loudness statistics in the IAMF file, measure the
    loudness of the input audio for at least the stereo downmix, and then modify
    the `integrated_loudness` and `digital_peak` values in the examples below.
    Tools such as the `loudnorm` and `astats` ffmpeg filters may be helpful for
    measuring the loudness.

### Encoding stereo wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT]" \
    -map "[FRONT]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 5.1 wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[BACK];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[BACK]" -map "[CENTER]" -map "[LFE]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=channel,layer=ch_layout=5.1(side)" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=5.1:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 5.1.2 wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[BACK];[0:a]channelmap=6|7:stereo[TOP_FRONT];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[BACK]" -map "[TOP_FRONT]" -map "[CENTER]" -map "[LFE]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:audio_element_type=channel,layer=ch_layout=5.1.2" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=5.1.2:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 7.1.4 wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[SIDE];[0:a]channelmap=6|7:stereo[BACK];[0:a]channelmap=8|9:stereo[TOP_FRONT];[0:a]channelmap=10|11:stereo[TOP_BACK];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[SIDE]" -map "[BACK]" -map "[TOP_FRONT]" -map "[TOP_BACK]" -map "[CENTER]" -map "[LFE]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:audio_element_type=channel,layer=ch_layout=7.1.4" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=7.1.4:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 1st order Ambisonics wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=scene,layer=ch_layout=ambisonic\ 1:ambisonics_mode=mono," \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=FOA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 3rd order Ambisonics wav to IAMF

Replace `/path/to/input.wav`, `/path/to/video.mp4` and `/path/to/output.mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3];[0:a]channelmap=4:mono[A4];[0:a]channelmap=5:mono[A5];[0:a]channelmap=6:mono[A6];[0:a]channelmap=7:mono[A7];[0:a]channelmap=8:mono[A8];[0:a]channelmap=9:mono[A9];[0:a]channelmap=10:mono[A10];[0:a]channelmap=11:mono[A11];[0:a]channelmap=12:mono[A12];[0:a]channelmap=13:mono[A13];[0:a]channelmap=14:mono[A14];[0:a]channelmap=15:mono[A15]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map "[A4]" -map "[A5]" -map "[A6]" -map "[A7]" -map "[A8]" -map "[A9]" -map "[A10]" -map "[A11]" -map "[A12]" -map "[A13]" -map "[A14]" -map "[A15]" -map 1:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:st=7:st=8:st=9:st=10:st=11:st=12:st=13:st=14:st=15:audio_element_type=scene,layer=ch_layout=ambisonic\ 3:ambisonics_mode=mono," \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=3OA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 -streamid 8:8 -streamid 9:9 -streamid 10:10 -streamid 11:11 -streamid 12:12 -streamid 13:13 -streamid 14:14 -streamid 15:15 -streamid 16:16 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 1st order Ambisonics and stereo wavs to IAMF

This example takes 2 input wav files. Replace the following:

-   `/path/to/input_FOA.wav`
-   `/path/to/input_stereo.wav`
-   `/path/to/video.mp4`
-   `/path/to/output.mp4`

By default, the `headphones_rendering_mode` is set to `binaural` for the
ambisonics input, and `stereo` for the stereo input, with the assumption that
the stereo input is a non-diegetic sound source. If both sound sources should be
binauralized instead of downmixed to stereo, change both instances of
`headphones_rendering_mode` to `binaural`.

```shell
ffmpeg -i /path/to/input_FOA.wav -i /path/to/input_stereo.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3]" \
    -filter_complex "[1:a]channelmap=0|1:stereo[FRONT]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map "[FRONT]" -map 2:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=scene,layer=ch_layout=ambisonic\ 1:ambisonics_mode=mono," \
    -stream_group "type=iamf_audio_element:id=2:st=4:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:stg=1:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=FOA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|element=stg=1:headphones_rendering_mode=stereo:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

### Encoding 3rd order Ambisonics and stereo wavs to IAMF

This example takes 2 input wav files. Replace the following:

-   `/path/to/input_TOA.wav`
-   `/path/to/input_stereo.wav`
-   `/path/to/video.mp4`
-   `/path/to/output.mp4`

By default, the `headphones_rendering_mode` is set to `binaural` for the
ambisonics input, and `stereo` for the stereo input, with the assumption that
the stereo input is a non-diegetic sound source. If both sound sources should be
binauralized instead of downmixed to stereo, change both instances of
`headphones_rendering_mode` to `binaural`.

```shell
ffmpeg -i /path/to/input_TOA.wav -i /path/to/input_stereo.wav \
    -i /path/to/video.mp4 -c:v copy \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3];[0:a]channelmap=4:mono[A4];[0:a]channelmap=5:mono[A5];[0:a]channelmap=6:mono[A6];[0:a]channelmap=7:mono[A7];[0:a]channelmap=8:mono[A8];[0:a]channelmap=9:mono[A9];[0:a]channelmap=10:mono[A10];[0:a]channelmap=11:mono[A11];[0:a]channelmap=12:mono[A12];[0:a]channelmap=13:mono[A13];[0:a]channelmap=14:mono[A14];[0:a]channelmap=15:mono[A15]" \
    -filter_complex "[1:a]channelmap=0|1:stereo[FRONT]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map "[A4]" -map "[A5]" -map "[A6]" -map "[A7]" -map "[A8]" -map "[A9]" -map "[A10]" -map "[A11]" -map "[A12]" -map "[A13]" -map "[A14]" -map "[A15]" -map "[FRONT]" -map 2:0 \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:st=7:st=8:st=9:st=10:st=11:st=12:st=13:st=14:st=15:audio_element_type=scene,layer=ch_layout=ambisonic\ 3:ambisonics_mode=mono," \
    -stream_group "type=iamf_audio_element:id=2:st=16:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:stg=1:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=3OA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|element=stg=1:headphones_rendering_mode=stereo:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0:digital_peak=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 -streamid 8:8 -streamid 9:9 -streamid 10:10 -streamid 11:11 -streamid 12:12 -streamid 13:13 -streamid 14:14 -streamid 15:15 -streamid 16:16 -streamid 17:17 \
    -c:a libopus -b:a 64000 /path/to/output.mp4
```

## Merge an IAMF file with video into MP4 using ffmpeg

The [FFmpeg](https://www.ffmpeg.org/) CLI can be used to merge an IAMF file with
another video file into an MP4 file.

To create the MP4 file, ffmpeg requires that the `stream_groups` and `streamid`s
from an input IAMF file are copied. The example below merges an IAMF file
containing 3rd order Ambisonics and stereo with a video file.

```shell
ffmpeg -i /path/to/3OA_and_stereo_iamf.mp4 \
    -i /path/to/video.mp4 \
    -c:v copy -c:a copy \
    -map 0:a:0 -map 0:a:1 -map 0:a:2 -map 0:a:3 -map 0:a:4 -map 0:a:5 -map 0:a:6 -map 0:a:7 -map 0:a:8 -map 0:a:9 -map 0:a:10 -map 0:a:11 -map 0:a:12 -map 0:a:13 -map 0:a:14 -map 0:a:15 -map 0:a:16 -map 1:v:0 \
    -stream_group map=0=0:st=0:st=1:st=2:st=3:st=4:st=5:st=6:st=7:st=8:st=9:st=10:st=11:st=12:st=13:st=14:st=15 \
    -stream_group map=0=1:st=16 \
    -stream_group map=0=2 \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 -streamid 8:8 -streamid 9:9 -streamid 10:10 -streamid 11:11 -streamid 12:12 -streamid 13:13 -streamid 14:14 -streamid 15:15 -streamid 16:16 \
    /path/to/output.mp4
```
