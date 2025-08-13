# IAMF Decoder

The [Immersive Audio Model and Formats](https://aomediacodec.github.io/iamf/)
(IAMF) decoder within `iamf-tools` is an implementation of a decoder in line
with the `IAMF` standard, as defined by the Alliance for Open Media (AOM).

## Purpose

The `IamfDecoderInterface` decodes valid `.iamf` files into audio (pcm). It is
designed to be iterative, meaning you can decode on a frame-by-frame basis. This
is especially useful for streaming applications.

## Sample Usage

Use cases for the IAMF decoder fall into two primary buckets, which we term
"Containerized" vs "Standalone" decoding. Sample usage is provided here for both
cases.

### Containerized Decoding

Containerized IAMF decoding refers to a use-case of the iamf-tools decoder in
which you know the boundaries of your input bitstream (an encoded .iamf stream)
well. This means that you know which bits refer to
[Descriptor Obus](https://aomediacodec.github.io/iamf/#bitstream-descriptors)
and which refer to
[Temporal Units](https://aomediacodec.github.io/iamf/#temporal-unit).
Additionally, you know the boundaries between each temporal unit as well. mp4
decoding falls into this use-case. Containerized decoding also allows you to use
some enhanced features; this includes dynamic playback layout switching,
seeking, and no-delay decoding. Delay here refers to having to wait for the next
OBU to decode the current one; this is necessary in Standalone Decoding, but is
not necessary in the Containerized mode.

```c++
IamfDecoderSettings settings = {
   .requested_layout = OutputLayout::kItu2051_SoundSystemA_0_2_0,
};
std::unique_pointer<IamfDecoderInterface> decoder = IamfDecoderInterface::CreateFromDescriptors(settings, input_buffer_descriptors, input_buffer_size);

// Methods to confirm decoder settings are as desired before decoding:
decoder.GetOutputLayout(output_layout);
decoder.GetNumOutputChannels(output_num_channels);
decoder.GetOutputSampleType();
decoder.GetSampleRate(output_sample_rate);
decoder.GetFrameSize(output_frame_size);

for chunk of data in iamf stream {
    decoder.Decode(chunk, chunk_size)
    while (decoder.IsTemporalUnitAvailable()) {
      decoder.GetOutputTemporalUnit(output_buffer, bytes_written)
      Playback(output_buffer)
    }
}
if (end_of_stream) {
  decoder.SignalEndOfStream()
  // Get remaining audio
  while (decoder.IsTemporalUnitAvailable()) {
    decoder.GetOutputTemporalUnit(output_buffer, bytes_written)
    Playback(output_buffer)
  }
}
```

Containerized Decoding also supports dynamic playback layout switching. This
enables you to change the playback layout (e.g. from stereo to 5.1) midstream.
After decoding some audio, call:

`decoder.ResetWithNewLayout(desired_layout)`

Or, if seeking is desired, call:

`decoder.Reset()`

In both cases, you can then simply continue decoding.

### Standalone Decoding

Standalone decoding is the pure-streaming case of IAMF decoding. In this
scenario, you have no knowledge of any boundaries within the IAMF bitstream - it
is the raw input. You therefore do not know the boundaries of the various items
listed above.

```c++
IamfDecoderSettings settings = {
   .requested_layout = OutputLayout::kItu2051_SoundSystemA_0_2_0,
};
std::unique_pointer<IamfDecoderInterface> decoder = IamfDecoderInterface::Create(settings);
for chunk of data in iamf stream {
  decoder.Decode(chunk, chunk_size)
  if (IsDescriptorProcessingComplete()) {
    // Methods to confirm decoder settings are as desired before decoding.
    // These methods can only be called after descriptors have been processed.
    decoder.GetOutputLayout(output_layout);
    decoder.GetNumOutputChannels(output_num_channels);
    decoder.GetOutputSampleType();
    decoder.GetSampleRate(output_sample_rate);
    decoder.GetFrameSize(output_frame_size);
  }
}
for chunk of data in iamf stream {
    decoder.Decode(chunk, chunk_size)
    while (decoder.IsTemporalUnitAvailable()) {
      decoder.GetOutputTemporalUnit(output_buffer, bytes_written)
      Playback(output_buffer)
    }
}
if (end_of_stream) {
  decoder.SignalEndOfStream()
  // Get remaining audio
  while (decoder.IsTemporalUnitAvailable()) {
    decoder.GetOutputTemporalUnit(output_buffer, bytes_written)
    Playback(output_buffer)
  }
}
```

Seeking or dynamic playback layout switching is not supported in standalone
mode.
