package(default_visibility = [":__subpackages__"])

cc_library(
    name = "cli_test_utils",
    testonly = True,
    srcs = ["cli_test_utils.cc"],
    hdrs = ["cli_test_utils.h"],
    deps = [
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:lpcm_decoder_config",
        "//iamf:mix_presentation",
        "//iamf:obu_header",
        "//iamf:opus_decoder_config",
        "//iamf:param_definitions",
        "//iamf/cli:audio_element_with_data",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "encoder_test_base",
    testonly = True,
    srcs = ["encoder_test_base.cc"],
    hdrs = ["encoder_test_base.h"],
    deps = [
        "//iamf:audio_frame",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:encoder_base",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "aac_encoder_test",
    srcs = ["aac_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf:aac_decoder_config",
        "//iamf:codec_config",
        "//iamf:obu_header",
        "//iamf/cli:aac_encoder_decoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "arbitrary_obu_generator_test",
    srcs = ["arbitrary_obu_generator_test.cc"],
    deps = [
        "//iamf:arbitrary_obu",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf/cli:arbitrary_obu_generator",
        "//iamf/cli/proto:arbitrary_obu_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "audio_element_generator_test",
    srcs = ["audio_element_generator_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf/cli:audio_element_generator",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli/proto:audio_element_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "audio_frame_decoder_test",
    srcs = ["audio_frame_decoder_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:audio_frame_decoder",
        "//iamf/cli:audio_frame_with_data",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "audio_frame_generator_test",
    srcs = ["audio_frame_generator_test.cc"],
    deps = [
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf/cli:audio_element_generator",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:audio_frame_generator",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:codec_config_generator",
        "//iamf/cli:demixing_module",
        "//iamf/cli:global_timing_module",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli:parameters_manager",
        "//iamf/cli/proto:audio_element_cc_proto",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "cli_util_test",
    size = "small",
    srcs = ["cli_util_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:cli_util",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:parameter_data_cc_proto",
        "//iamf/cli/proto:temporal_delimiter_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "codec_config_generator_test",
    srcs = ["codec_config_generator_test.cc"],
    deps = [
        "//iamf:aac_decoder_config",
        "//iamf:codec_config",
        "//iamf:flac_decoder_config",
        "//iamf:ia",
        "//iamf:lpcm_decoder_config",
        "//iamf:obu_header",
        "//iamf:opus_decoder_config",
        "//iamf/cli:codec_config_generator",
        "//iamf/cli/proto:codec_config_cc_proto",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "demixing_module_test",
    srcs = ["demixing_module_test.cc"],
    deps = [
        "//iamf:audio_element",
        "//iamf:audio_frame",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:audio_frame_decoder",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:demixing_module",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoder_base_test",
    srcs = ["encoder_base_test.cc"],
    deps = [
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:encoder_base",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoder_main_lib_test",
    srcs = ["encoder_main_lib_test.cc"],
    deps = [
        "//iamf/cli:encoder_main_lib",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "flac_encoder_test",
    srcs = ["flac_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf:codec_config",
        "//iamf:flac_decoder_config",
        "//iamf:obu_header",
        "//iamf/cli:flac_encoder_decoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "global_timing_module_test",
    srcs = ["global_timing_module_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:param_definitions",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:global_timing_module",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "ia_sequence_header_generator_test",
    srcs = ["ia_sequence_header_generator_test.cc"],
    deps = [
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:obu_header",
        "//iamf/cli:ia_sequence_header_generator",
        "//iamf/cli/proto:ia_sequence_header_cc_proto",
        "//iamf/cli/proto:obu_header_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "iamf_components_test",
    srcs = ["iamf_components_test.cc"],
    deps = [
        "//iamf/cli:iamf_components",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "leb_generator_test",
    srcs = ["leb_generator_test.cc"],
    deps = [
        "//iamf:ia",
        "//iamf/cli:leb_generator",
        "//iamf/cli/proto:test_vector_metadata_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "lpcm_encoder_test",
    size = "small",
    srcs = ["lpcm_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf:codec_config",
        "//iamf:lpcm_decoder_config",
        "//iamf:obu_header",
        "//iamf/cli:lpcm_encoder",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mix_presentation_finalizer_test",
    srcs = ["mix_presentation_finalizer_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:mix_presentation",
        "//iamf/cli:mix_presentation_finalizer",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "mix_presentation_generator_test",
    srcs = ["mix_presentation_generator_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:mix_presentation",
        "//iamf/cli:mix_presentation_generator",
        "//iamf/cli/proto:mix_presentation_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "obu_sequencer_test",
    srcs = ["obu_sequencer_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:arbitrary_obu",
        "//iamf:audio_frame",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_base",
        "//iamf:obu_header",
        "//iamf:write_bit_buffer",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:obu_sequencer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "opus_encoder_test",
    srcs = ["opus_encoder_test.cc"],
    deps = [
        ":encoder_test_base",
        "//iamf:codec_config",
        "//iamf:obu_header",
        "//iamf:opus_decoder_config",
        "//iamf/cli:opus_encoder_decoder",
        "//iamf/cli/proto:codec_config_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameter_block_generator_test",
    size = "small",
    srcs = ["parameter_block_generator_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:audio_element",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:ia_sequence_header",
        "//iamf:mix_presentation",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:demixing_module",
        "//iamf/cli:global_timing_module",
        "//iamf/cli:parameter_block_generator",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli/proto:parameter_block_cc_proto",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "parameter_block_partitioner_test",
    size = "small",
    srcs = ["parameter_block_partitioner_test.cc"],
    deps = [
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli:parameter_block_partitioner",
        "//iamf/cli:parameter_block_with_data",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parameters_manager_test",
    size = "small",
    srcs = ["parameters_manager_test.cc"],
    deps = [
        ":cli_test_utils",
        "//iamf:codec_config",
        "//iamf:demixing_info_param_data",
        "//iamf:ia",
        "//iamf:obu_header",
        "//iamf:param_definitions",
        "//iamf:parameter_block",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli:parameters_manager",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "recon_gain_generator_test",
    srcs = ["recon_gain_generator_test.cc"],
    deps = [
        "//iamf/cli:demixing_module",
        "//iamf/cli:recon_gain_generator",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wav_reader_test",
    srcs = ["wav_reader_test.cc"],
    data = [
        "//iamf/cli/testdata:sine_1000_16khz_512ms_s32le.wav",
        "//iamf/cli/testdata:stereo_8_samples_48khz_s16le.wav",
        "//iamf/cli/testdata:stereo_8_samples_48khz_s24le.wav",
    ],
    deps = [
        "//iamf/cli:wav_reader",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wav_sample_provider_test",
    srcs = ["wav_sample_provider_test.cc"],
    data = [
        "//iamf/cli/testdata:stereo_8_samples_48khz_s16le.wav",
        "//iamf/cli/testdata:stereo_8_samples_48khz_s24le.wav",
    ],
    deps = [
        ":cli_test_utils",
        "//iamf:codec_config",
        "//iamf:ia",
        "//iamf/cli:audio_element_with_data",
        "//iamf/cli:demixing_module",
        "//iamf/cli:wav_sample_provider",
        "//iamf/cli/proto:user_metadata_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "wav_writer_test",
    srcs = ["wav_writer_test.cc"],
    deps = [
        "//iamf/cli:wav_writer",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest_main",
    ],
)
