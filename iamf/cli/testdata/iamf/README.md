# Small (<5kB) IAMF files

Filename                         | Contents                                                    | Duration     | Layout       | Encoding
-------------------------------- | ----------------------------------------------------------- | ------------ | ------------ | --------
noise_1024samp_5p1_opus.iamf     | White noise, same all channels                              | 1024 samples | 5.1          | Opus
noise_1024samp_stereo_flac.iamf  | White noise, different L/R                                  | 1024 samples | stereo       | FLAC
noise_3s_stereo_opus.iamf        | White noise, different L/R                                  | 3 seconds    | stereo       | Opus
tones_100ms_1OA_stereo_opus.iamf | Tones (Hz) A0: 111, A1: 222, A2: 333, A3: 444; L: 8888, R: 9999 | 100ms        | 1OA (mono mode) + stereo | Opus
tones_256samp_5p1_pcm.iamf       | Tone (Hz) L: 220, R: 202, C: 303, LFE: 65, Ls: 434, Rs: 616 | 256 samples  | 5.1          | PCM
