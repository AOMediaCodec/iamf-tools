package(default_visibility = [
    "//iamf/api/decoder/tests:__pkg__",
])

# keep-sorted start block=yes prefix_order=cc_library newline_separated=yes
cc_library(
    name = "iamf_decoder",
    srcs = ["iamf_decoder.cc"],
    hdrs = ["iamf_decoder.h"],
    deps = [
        "//iamf/cli:audio_frame_with_data",
        "//iamf/cli:obu_processor",
        "//iamf/cli:parameter_block_with_data",
        "//iamf/cli:rendering_mix_presentation_finalizer",
        "//iamf/common:read_bit_buffer",
        "//iamf/common/utils:macros",
        "//iamf/obu:mix_presentation",
        "//iamf/obu:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

# keep-sorted end
