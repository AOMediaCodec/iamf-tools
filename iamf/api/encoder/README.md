# IAMF Encoder

The [Immersive Audio Model and Formats](https://aomediacodec.github.io/iamf/)
(IAMF) encoder within `iamf-tools` is designed to create IAMF bitstreams which
are compliant with the `IAMF` standard, as defined by the Alliance for Open
Media (AOM).

## Purpose

The `IamfEncoderInterface` encodes an IAMF bitstream based on various
configuration settings and input audio. It supports encoding on a frame-by-frame
basis. This is especially useful for streaming applications.

## Creation

`IamfEncoderFactory` provides functions to create an encoder.

-   `CreateIamfEncoder` is useful to directly stream IAMF content.
-   `CreateFileGeneratingEncoder` is useful to automatically produce a `.iamf`
    file.

Either creation mode is configured by a `UserMetadata`. Many properties of the
IAMF bitstream can be changed. See
[Textproto templates](iamf/cli/textproto_templates) for common configurations.

## Sample Usage

In either case, the encoder is used in a similar manner. The below code uses a
`streaming` variable to differentiate these cases.

### Prepare

Prepare the encoder and any buffers which can be reused between frames.

```c++
 // Get an encoder.
std::unique_ptr<IamfEncoderInterface> encoder = ...;
 // Reusable buffer, later redundant copies won't change size.
std::vector<uint8_t> descriptor_obus;
// When writing directly to a file `CreateFileGeneratingEncoder`, descriptor
// OBUS are automatically managed. When streaming `CreateIamfEncoder`,
// descriptor OBUs should be broadcast periodically to help downstream clients
// synchronize to a stream.
bool streaming = ...;
if (streaming) {
  bool kNoRedundantCopy = false;
  // Broadcast the first set of descriptor OBUs, to allow consumers to sync.
  // Using `CreateFileGeneratingEncoder` automatically handles descriptors. If
  // not streaming it is safe to skip the call.
  RETURN_IF_NOT_OK(
      encoder->GetDescriptorObus(kNoRedundantCopy, descriptor_obus));
}

// Reusable input buffer. Hardcode an LFE and stereo audio element to show
// multi-dimensionality. Hardcode 1024 samples per frame. The same
// backing allocation can be used for each frame.
using ChannelLabel::Label;
const absl::Span<const double> kEmptyFrame;
IamfTemporalUnitData temporal_unit_data = {
    .parameter_block_id_to_metadata = {},
    .audio_element_id_to_data =
        {
            {0, {{LFE, kEmptyFrame}}},
            {1, {{L2, kEmptyFrame}, {R2, kEmptyFrame}}},
        },
};
// Reusable buffer. It will grow towards the maximum size of an output
//  temporal unit.
std::vector<uint8_> temporal_unit_obus;
// Repeat descriptors every so often to help clients sync. In practice,
// an API user would determine something based on their use case. For
// example to aim for ~5 seconds of output audio between descriptors.
const int kDescriptorRepeatInterval = 100;
```

### Process temporal units

Process temporal units until this is no more input audio.

```c++
while (encoder->GeneratingTemporalUnits()) {
  if (streaming && iteration_count % kDescriptorRepeatInterval == 0) {
    bool kRedundantCopy = true;
    // Broadcast the redundant descriptor OBUs.
    RETURN_IF_NOT_OK(
        encoder->GetDescriptorObus(kRedundantCopy, descriptor_obus));
  }
  // Fill `temporal_unit_data` for this frame.
  for (each audio element) {
    for (each channel label from the current element) {
      // Fill the slot in `temporal_unit_data` for this audio element and
      // channel label.
    }
  }
  // Fill any parameter blocks.
  for (each parameter block metadata) {
    // Fill the slot in `temporal_unit_data` for this parameter block.
  }

  RETURN_IF_NOT_OK(encoder->Encode(temporal_unit_data));

  if (done_receiving_all_audio) {
    encoder->FinalizeEncode();
  }

  // Flush OBUs for the next temporal unit.
  encoder->OutputTemporalUnit(temporal_unit_obus);
  if (streaming) {
    // Broadcast the temporal unit descriptor OBUs.
  }
  // Otherwise, the temporal units were automatically flushed to the file.
}
```

### Cleanup

```c++
// Data generation is done. Perform some cleanup.
if (streaming) {
  bool kNoRedundantCopy = false;
  RETURN_IF_NOT_OK(
      encoder->GetDescriptorObus(kNoRedundantCopy, descriptor_obus));
  // If any consumers require accurate descriptors (loudness), notify them.
}
// Otherwise, they were flushed to file after the last temporal unit was output.
```
