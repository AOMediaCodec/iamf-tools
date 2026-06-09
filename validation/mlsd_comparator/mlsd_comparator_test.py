# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
import os

import numpy as np
from scipy.io import wavfile

from absl.testing import absltest
from validation.mlsd_comparator import mlsd_comparator


class MlsdComparatorTest(googletest.TestCase):

  def test_calc_per_channel_lsd_pcm_identical(self):
    sampling_rate = 48000
    num_samples = 48000
    num_channels = 2
    ref_signal = np.zeros((num_samples, num_channels), dtype=np.int16)
    signal = np.zeros((num_samples, num_channels), dtype=np.int16)

    lsd_list = mlsd_comparator.calc_per_channel_lsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(lsd_list, num_channels)
    for lsd in lsd_list:
      self.assertAlmostEqual(lsd, 0.0)

  def test_calc_per_channel_lsd_pcm_mono(self):
    sampling_rate = 48000
    num_samples = 48000
    ref_signal = np.zeros(num_samples, dtype=np.int16)
    signal = np.zeros(num_samples, dtype=np.int16)

    lsd_list = mlsd_comparator.calc_per_channel_lsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(lsd_list, 1)
    self.assertAlmostEqual(lsd_list[0], 0.0)

  def test_calc_per_channel_lsd_pcm_different(self):
    sampling_rate = 48000
    num_samples = 48000
    num_channels = 2
    ref_signal = np.zeros((num_samples, num_channels), dtype=np.int16)
    signal = np.ones((num_samples, num_channels), dtype=np.int16) * 1000

    lsd_list = mlsd_comparator.calc_per_channel_lsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(lsd_list, num_channels)
    for lsd in lsd_list:
      self.assertGreater(lsd, 0.0)

  def _create_dummy_wav(self, filename, sampling_rate, data):
    path = os.path.join(self.create_tempdir().full_path, filename)
    wavfile.write(path, sampling_rate, data)
    return path

  def test_calc_score_wav_different_samplerate(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 44100, np.zeros((48000, 1), dtype=np.int16)
    )

    with self.assertRaisesRegex(ValueError, "Sampling rate .* are different"):
      mlsd_comparator.calc_score_wav(ref_path, test_path)

  def test_calc_score_wav_different_channels(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((48000, 2), dtype=np.int16)
    )

    with self.assertRaisesRegex(
        ValueError, "Number of channels .* are different"
    ):
      mlsd_comparator.calc_score_wav(ref_path, test_path)

  def test_calc_score_wav_different_length(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((24000, 1), dtype=np.int16)
    )

    with self.assertRaisesRegex(
        ValueError, "Number of samples .* are different"
    ):
      mlsd_comparator.calc_score_wav(ref_path, test_path)

  def test_calc_score_wav_different_bit_depth(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((48000, 1), dtype=np.int32)
    )

    with self.assertRaisesRegex(ValueError, "Bit depth .* are different"):
      mlsd_comparator.calc_score_wav(ref_path, test_path)


if __name__ == "__main__":
  absltest.main()
