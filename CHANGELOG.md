# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Removed

-   Drop support `channel_ids` and `channel_labels` in `AudioElementObuMetadata`
    in favor of `ChannelMetadata`.
-   Drop support of `language_labels` in favor of `annotations_language`.
-   Drop support of `MixPresentationAnnotations` and
    `mix_presentation_annotations_array` in favor of
    `localized_presentation_annotations`.
-   Drop support of `MixPresentationElementAnnotations` and
    `mix_presentation_element_annotations_array` in favor of
    `localized_element_annotations`.

## [2.1.0] - 2025-11-06

### Added

-   Add an [example binary](docs/iamf_decoder_main.md) to decode IAMF.

### Deprecated

-   Deprecate `MixPresentationObuMetadata::count_label`, in favor of calculating
    the size from related elements.
-   Deprecate `MixPresentationObuMetadata::info_type_size`, in favor of
    calculating the size from related elements.
-   Deprecate `MixPresentationObuMetadata::rendering_config_extension_size`, in
    favor of calculating the size from related elements.
-   Deprecate `FlacMetaBlockHeader::last_metadata_block_flag` and
    `metadata_data_block_length` in favor of automatically determining the
    related values.
-   Deprecate `param_definition_size`, in favor of calculating the size from
    related elements.
-   Deprecate `param_data_size`, in favor of calculating the size from related
    elements.

### Changed

-   Changed encoder API to take in serialized protos. The previous API was buggy
    due to directly taking in C++ proto classes, which is
    [strongly discouraged](https://protobuf.dev/support/cross-version-runtime-guarantee/).
-   Update several library dependencies.
-   Use `ABSL_LOG` and `ABSL_CHECK` family of functions, to avoid conflicts with
    users that need other logging libraries.
-   Avoid use of global static initializers.
-   Fix issues when building under `-Wnullability-completeness`.

### Fixed

-   Fix a bug where the encoder would fail to finalize loudness under certain
    circumstances.
-   Improve spec compliance, when decoding streams with unknown parameter
    blocks.
-   Improve CPU/memory efficiency when decoding streams with multiple layouts
    and/or mix presentations.

### Security

-   Fix potential buffer overflows, invalid memory access, or excessive memory
    usage for certain bitstreams.

## [2.0.0] - 2025-08-18

### Added

-   Add a library to process ADM files into `UserMetadata`.
-   Add support for ADM input in the encoder.
-   Add support for binary proto input in the encoder.
-   Add support for encoding [Standalone IAMF Representation] for Base-Enhanced.
    profile based on [IAMF v1.1.0]("Expanded" layouts, Mix Presentation Tags).
-   Add an iterative [encoding API](docs/iamf_encoder_interface.md).
-   Add an iterative [decoding API](docs/iamf_decoder_interface.md).

### Deprecated

-   Deprecate several fields and prefer using field names based on
    [IAMF v1.1.0]:
    -   Deprecate `language_labels` in favor of `annotations_language`.
    -   Deprecate `MixPresentationAnnotations` and
        `mix_presentation_annotations_array` in favor of
        `localized_presentation_annotations`.
    -   Deprecate `MixPresentationElementAnnotations` and
        `mix_presentation_element_annotations_array` in favor of
        `localized_element_annotations`.
    -   Deprecate `OutputMixConfig` and `output_mix_config` in favor of
        `output_mix_gain`.
    -   Deprecate `ElementMixConfig` and `element_mix_config` in favor of
        `element_mix_gain`.
-   Deprecate `channel_ids` and `channel_labels` in `AudioElementObuMetadata` in
    favor of `ChannelMetadata`.
-   Deprecate several fields which redundantly provided the size. In favor of
    calculating the size from related elements.
    -   Deprecate `num_substreams`, `num_parameters`, `num_layers` in
        `AudioElementObuMetadata`.
    -   Deprecate `num_subblocks` in `ParamDefinition` and
        `ParameterBlockMetadata`.
    -   Deprecate `extension_header_size` in `ObuHeaderMetadata`.
    -   Deprecate `num_sub_mixes`, `num_audio_elements`, `num_layouts`,
        `num_anchored_loudness`, and `num_tags` in `MixPresentationObuMetadata`.

### Removed

-   Remove support for integral `deprecated_codec_id`, `deprecated_info_type`,
    `deprecated_param_definition_type`, `deprecated_loudspeaker_layout` in favor
    of enumeration-based fields.

### Changed

-   Set sensible defaults for some proto fields.
-   Default to automatically determining the correct
    `CodecConfig::audio_roll_distance`, instead of throwing an error when user
    input was incorrect.
-   Default to automatically determining the correct
    `OpusDecoderConfig::pre_skip`, instead of throwing an error when user input
    was incorrect.
-   Update Simple and Base profile to be based on [IAMF v1.0.0-errata].
-   Update several library dependencies.

### Fixed

-   Fix parsing multi-byte UTF-8 characters on certain platforms.
-   Fix crashes when attempting to encode audio frames without the correct
    number of `substream_id`s.
-   Prevent encoding Mix Presentation OBUs with an inconsistent number of
    annotations.
-   Fix compliance with ISO 14496-1:2010 when writing AAC Codec Config OBUs
    (AOMediaCodec/libiamf#119).
-   Fix issues decoding FLAC
    [https://github.com/AOMediaCodec/iamf-tools/commit/8c1a71378].
-   Fix issues when using AAC with a 24 kHz sample rate.
-   Permit one fully trimmed audio frame at the end of a substream.
-   Fix several CPU and memory inefficiencies to improve performance.
-   Avoid use of run-time type information (RTTI).

## [1.0.0] - 2024-01-26

### Added

-   Add an IAMF encoder which takes in `UserMetadata` and outputs IAMF files.
-   Add support for encoding [Standalone IAMF Representation] for Simple and
    Base profiles based on [IAMF v1.0.0].
-   Fork a test suite from
    [`libiamf`](https://github.com/AOMediaCodec/libiamf/commit/f9cdea5c).
    -   `*.proto`: A schema to describe IA Sequences and metadata to process
        them. `UserMetadata` is the "top-level" file.
    -   `*.textproto`: A suite of `UserMetadata` textproto files to generate
        test IAMF files.

### Deprecated

-   Deprecate `deprecated_codec_id`, `deprecated_info_type`,
    `deprecated_param_definition_type`, `deprecated_loudspeaker_layout` from the
    forked `.protos`.

[Unreleased]: https://github.com/AOMediaCodec/iamf-tools/compare/v2.1.0...HEAD
[2.1.0]: https://github.com/AOMediaCodec/iamf-tools/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/AOMediaCodec/iamf-tools/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/AOMediaCodec/iamf-tools/releases/tag/v1.0.0
[Standalone IAMF Representation]: https://aomediacodec.github.io/iamf/#standalone
[IAMF v1.0.0]: https://aomediacodec.github.io/iamf/v1.0.0.html
[IAMF v1.0.0-errata]: https://aomediacodec.github.io/iamf/v1.0.0.html
[IAMF v1.1.0]: https://aomediacodec.github.io/iamf/v1.1.0.html
