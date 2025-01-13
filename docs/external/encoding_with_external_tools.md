# Encoding with external tools

The [FFmpeg](https://www.ffmpeg.org/) CLI can be used to encode a set of input
wav files to IAMF and merge with a video file into an MP4 file. Some example
commands are listed below. See the [official ffmpeg
documentation](https://www.ffmpeg.org/ffmpeg.html) for more details.

## Encode wav files to IAMF with ffmpeg

The [FFmpeg](https://www.ffmpeg.org/) CLI can be used to encode a set of
input wav files into a standalone IAMF Sequence (`.iamf`) file or IAMF
encapsulated in MP4.

The examples below encode input wav files to IAMF using Opus to encode the
underlying audio elements at 64 kbps per channel.

> **NOTE: Input channel order**
>
> The examples assume that the channel order in the input wav files follow the
> ordering used in [ITU-R BS.2051](https://www.itu.int/rec/R-REC-BS.2051).
>
> If a different channel order is used, change the input channel indices
> indicated by `channelmap`. The [IAMF specification (Coupled stereo
> channels)](https://aomediacodec.github.io/iamf/v1.1.0.html#coupled_substream_count)
> requires that the result groups specific channels as either a stereo pair or
> a mono channel, and additionally that they follow a specific order. In the
> ffmpeg command, this is defined by the order of the `-map "[]"` options.

### Encoding stereo wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT]" \
    -map "[FRONT]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 5.1 wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[BACK];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[BACK]" -map "[CENTER]" -map "[LFE]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=channel,layer=ch_layout=5.1" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=5.1:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 5.1.2 wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[BACK];[0:a]channelmap=6|7:stereo[TOP_FRONT];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[BACK]" -map "[TOP_FRONT]" -map "[CENTER]" -map "[LFE]"
    \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:audio_element_type=channel,layer=ch_layout=5.1.2" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=5.1.2:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 7.1.4 wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0|1:stereo[FRONT];[0:a]channelmap=4|5:stereo[SIDE];[0:a]channelmap=6|7:stereo[BACK];[0:a]channelmap=8|9:stereo[TOP_FRONT];[0:a]channelmap=10|11:stereo[TOP_BACK];[0:a]channelmap=2:mono[CENTER];[0:a]channelmap=3:mono[LFE]" \
    -map "[FRONT]" -map "[SIDE]" -map "[BACK]" -map "[TOP_FRONT]" -map "[TOP_BACK]" -map "[CENTER]" -map "[LFE]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:audio_element_type=channel,layer=ch_layout=7.1.4" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=7.1.4:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 1st order Ambisonics wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=scene,layer=ch_layout=ambisonic\ 1:ambisonics_mode=mono," \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=FOA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 3rd order Ambisonics wav to IAMF

Replace `/path/to/input.wav` and `/path/to/output_iamf_or_mp4`.

```shell
ffmpeg -i /path/to/input.wav \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3];[0:a]channelmap=4:mono[A4];[0:a]channelmap=5:mono[A5];[0:a]channelmap=6:mono[A6];[0:a]channelmap=7:mono[A7];[0:a]channelmap=8:mono[A8];[0:a]channelmap=9:mono[A9];[0:a]channelmap=10:mono[A10];[0:a]channelmap=11:mono[A11];[0:a]channelmap=12:mono[A12];[0:a]channelmap=13:mono[A13];[0:a]channelmap=14:mono[A14];[0:a]channelmap=15:mono[A15]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map "[A4]" -map "[A5]" -map "[A6]" -map "[A7]" -map "[A8]" -map "[A9]" -map "[A10]" -map "[A11]" -map "[A12]" -map "[A13]" -map "[A14]" -map "[A15]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:st=7:st=8:st=9:st=10:st=11:st=12:st=13:st=14:st=15:audio_element_type=scene,layer=ch_layout=ambisonic\ 3:ambisonics_mode=mono," \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=3OA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 -streamid 8:8 -streamid 9:9 -streamid 10:10 -streamid 11:11 -streamid 12:12 -streamid 13:13 -streamid 14:14 -streamid 15:15 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 1st order Ambisonics and stereo wavs to IAMF

This example takes 2 input wav files. Replace the following:

-   `/path/to/input_FOA.wav`
-   `/path/to/input_stereo.wav`
-   `/path/to/output_iamf_or_mp4`

By default, the `headphones_rendering_mode` is set to `binaural` for the
ambisonics input, and `stereo` for the stereo input, with the assumption that
the stereo input is a non-diegetic sound source. If both sound sources should be
binauralized instead of downmixed to stereo, change both instances of
`headphones_rendering_mode` to `binaural`.

```shell
ffmpeg -i /path/to/input_FOA.wav -i /path/to/input_stereo.wav \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:audio_element_type=scene,layer=ch_layout=ambisonic\ 1:ambisonics_mode=mono," \
    -filter_complex "[1:a]channelmap=0|1:stereo[FRONT]" \
    -map "[FRONT]" \
    -stream_group "type=iamf_audio_element:id=2:st=4:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:stg=1:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=FOA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|element=stg=1:headphones_rendering_mode=stereo:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

### Encoding 3rd order Ambisonics and stereo wavs to IAMF

This example takes 2 input wav files. Replace the following:

-   `/path/to/input_TOA.wav`
-   `/path/to/input_stereo.wav`
-   `/path/to/output_iamf_or_mp4`

By default, the `headphones_rendering_mode` is set to `binaural` for the
ambisonics input, and `stereo` for the stereo input, with the assumption that
the stereo input is a non-diegetic sound source. If both sound sources should be
binauralized instead of downmixed to stereo, change both instances of
`headphones_rendering_mode` to `binaural`.

```shell
ffmpeg -i /path/to/input_TOA.wav -i /path/to/input_stereo.wav \
    -filter_complex "[0:a]channelmap=0:mono[A0];[0:a]channelmap=1:mono[A1];[0:a]channelmap=2:mono[A2];[0:a]channelmap=3:mono[A3];[0:a]channelmap=4:mono[A4];[0:a]channelmap=5:mono[A5];[0:a]channelmap=6:mono[A6];[0:a]channelmap=7:mono[A7];[0:a]channelmap=8:mono[A8];[0:a]channelmap=9:mono[A9];[0:a]channelmap=10:mono[A10];[0:a]channelmap=11:mono[A11];[0:a]channelmap=12:mono[A12];[0:a]channelmap=13:mono[A13];[0:a]channelmap=14:mono[A14];[0:a]channelmap=15:mono[A15]" \
    -map "[A0]" -map "[A1]" -map "[A2]" -map "[A3]" -map "[A4]" -map "[A5]" -map "[A6]" -map "[A7]" -map "[A8]" -map "[A9]" -map "[A10]" -map "[A11]" -map "[A12]" -map "[A13]" -map "[A14]" -map "[A15]" \
    -stream_group "type=iamf_audio_element:id=1:st=0:st=1:st=2:st=3:st=4:st=5:st=6:st=7:st=8:st=9:st=10:st=11:st=12:st=13:st=14:st=15:audio_element_type=scene,layer=ch_layout=ambisonic\ 3:ambisonics_mode=mono," \
    -filter_complex "[1:a]channelmap=0|1:stereo[FRONT]" \
    -map "[FRONT]" \
    -stream_group "type=iamf_audio_element:id=2:st=16:audio_element_type=channel,layer=ch_layout=stereo" \
    -stream_group "type=iamf_mix_presentation:id=3:stg=0:stg=1:annotations=en-us=default_mix_presentation,submix=parameter_id=100:parameter_rate=48000:default_mix_gain=0.0|element=stg=0:headphones_rendering_mode=binaural:annotations=en-us=3OA:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|element=stg=1:headphones_rendering_mode=stereo:annotations=en-us=stereo:parameter_id=101:parameter_rate=48000:default_mix_gain=0.0|layout=sound_system=stereo:integrated_loudness=0.0" \
    -streamid 0:0 -streamid 1:1 -streamid 2:2 -streamid 3:3 -streamid 4:4 -streamid 5:5 -streamid 6:6 -streamid 7:7 -streamid 8:8 -streamid 9:9 -streamid 10:10 -streamid 11:11 -streamid 12:12 -streamid 13:13 -streamid 14:14 -streamid 15:15 -streamid 16:16 \
    -c:a libopus -b:a 64000 /path/to/output_iamf_or_mp4
```

## Merge an IAMF file with video into MP4 using ffmpeg

The [FFmpeg](https://www.ffmpeg.org/) CLI can be used to merge a standalone IAMF
sequence (`.iamf`) file with an existing video into an MP4 file.

To create the MP4 file, ffmpeg requires that the `stream_groups` and `streamid`s
from an input IAMF file are copied. The helper script below is an example of
how ffprobe could be used to detect the stream groups and stream ids in the
input IAMF file, and provided to ffmpeg to merge the video and IAMF file into an
output MP4 file.

```shell
#!/bin/bash

# Replace the following paths
input_iamf="/path/to/input.iamf"  # This may also be an MP4 with IAMF audio.
input_video="/path/to/input_video.mp4"
output_mp4="/path/to/output.mp4"

# ffmpeg needs to copy stream groups and stream ids when creating IAMF files.
# Use ffprobe to detect the stream groups and streams in the input IAMF file.
stream_groups_count=$(ffprobe \
                      -v error \
                      -select_streams a \
                      -show_entries stream=index \
                      -of json \
                      $input_iamf | \
                      jq '.stream_groups | length')

# Assumes there is at least one stream group, and that all stream groups have
# the same number of streams.
stream_count=$(ffprobe \
               -v error \
               -select_streams a \
               -show_entries stream=index \
               -of json \
               $input_iamf | \
               jq '.stream_groups[0]' | jq '.streams | length')

# Construct the partial commands to copy stream groups and stream IDs.
st_map=""
for (( i=0; i<$stream_count; i++ )); do
  st_map="$st_map:st=$i"
done

streamid_map=""
for (( i=0; i<$stream_count; i++ )); do
  streamid_map="$streamid_map -streamid $i:$i"
done

# Merge with the video file to create the final MP4 file.
ffmpeg \
  -i $input_iamf \
  -i $input_video \
  -c:a copy -map 0:a \
  -c:v copy -map 1:v:0 \
  -stream_group map=0=0$st_map \
  -stream_group map=0=1$st_map \
  $streamid_map \
  $output_mp4
```
