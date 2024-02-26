package(default_visibility = ["//iamf/cli:__subpackages__"])

exports_files([
    "sample1_48kHz_stereo.wav",
    "sawtooth_100_stereo.wav",
    "sawtooth_10000_foa_48khz.wav",
    "sawtooth_10000_stereo_48khz.wav",
    "sawtooth_10000_stereo_44100hz_s16le.wav",
    "sawtooth_10000_stereo_48khz_s24le.wav",
    "stereo_8_samples_48khz_s16le.wav",
    "stereo_8_samples_48khz_s24le.wav",
    "sawtooth_8000_toa_48khz.wav",
    "Mechanism_5s.wav",
    "Mechanism_5s_32bit.wav",
    "Transport_TOA_5s.wav",
    "audiolab-acoustic-guitar_2OA_470_ALLRAD_concat.wav",
    "audiolab-acoustic-guitar_2OA_470_ALLRAD_0.5s.wav",
    "audiolab-acoustic-guitar_2OA_470_ALLRAD_5s.wav",
    "sine_1000_48khz.wav",
    "sine_1000_48khz_512ms.wav",
    "sine_1000_16khz_512ms.wav",
    "sine_1000_16khz_512ms_s32le.wav",
    "dialog_clip_stereo.wav",
] + glob(
    [
        "test_*.textproto",
    ],
))

filegroup(
    name = "test_vectors",
    srcs = glob(["test_*.textproto"]),
)
