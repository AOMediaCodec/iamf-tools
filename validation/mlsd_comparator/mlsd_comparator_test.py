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

  def test_calc_per_channel_mlsd_pcm_identical(self):
    sampling_rate = 48000
    num_samples = 48000
    num_channels = 2
    ref_signal = np.zeros((num_samples, num_channels), dtype=np.int16)
    signal = np.zeros((num_samples, num_channels), dtype=np.int16)

    mlsd_list = mlsd_comparator.calc_per_channel_mlsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(mlsd_list, num_channels)
    for mlsd in mlsd_list:
      np.testing.assert_allclose(mlsd, 0.0, atol=1e-7)

  def test_calc_per_channel_mlsd_pcm_mono(self):
    sampling_rate = 48000
    num_samples = 48000
    ref_signal = np.zeros(num_samples, dtype=np.int16)
    signal = np.zeros(num_samples, dtype=np.int16)

    mlsd_list = mlsd_comparator.calc_per_channel_mlsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(mlsd_list, 1)
    np.testing.assert_allclose(mlsd_list[0], 0.0, atol=1e-7)

  def test_calc_per_channel_mlsd_pcm_different(self):
    sampling_rate = 48000
    num_samples = 48000
    num_channels = 2
    ref_signal = np.zeros((num_samples, num_channels), dtype=np.int16)
    signal = np.ones((num_samples, num_channels), dtype=np.int16) * 1000

    mlsd_list = mlsd_comparator.calc_per_channel_mlsd_pcm(
        ref_signal, signal, sampling_rate
    )

    self.assertLen(mlsd_list, num_channels)
    for mlsd in mlsd_list:
      self.assertGreater(np.min(mlsd), 0.0)

  def _create_dummy_wav(self, filename, sampling_rate, data):
    path = os.path.join(self.create_tempdir().full_path, filename)
    wavfile.write(path, sampling_rate, data)
    return path

  def test_evaluate_audio_quality_different_samplerate(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 44100, np.zeros((48000, 1), dtype=np.int16)
    )

    with self.assertRaisesRegex(ValueError, "Sampling rate .* are different"):
      mlsd_comparator.evaluate_audio_quality(ref_path, test_path)

  def test_evaluate_audio_quality_different_channels(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((48000, 2), dtype=np.int16)
    )

    with self.assertRaisesRegex(
        ValueError, "Number of channels .* are different"
    ):
      mlsd_comparator.evaluate_audio_quality(ref_path, test_path)

  def test_evaluate_audio_quality_different_length(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((24000, 1), dtype=np.int16)
    )

    with self.assertRaisesRegex(
        ValueError, "Number of samples .* are different"
    ):
      mlsd_comparator.evaluate_audio_quality(ref_path, test_path)

  def test_evaluate_audio_quality_different_bit_depth(self):
    ref_path = self._create_dummy_wav(
        "ref.wav", 48000, np.zeros((48000, 1), dtype=np.int16)
    )
    test_path = self._create_dummy_wav(
        "test.wav", 48000, np.zeros((48000, 1), dtype=np.int32)
    )

    with self.assertRaisesRegex(ValueError, "Bit depth .* are different"):
      mlsd_comparator.evaluate_audio_quality(ref_path, test_path)

  def test_evaluate_audio_quality_identical(self):
    # Create dummy sine wave
    t = np.linspace(0, 1, 48000, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t) * 10000
    data = sine.astype(np.int16)

    ref_path = self._create_dummy_wav("ref.wav", 48000, data)
    test_path = self._create_dummy_wav("test.wav", 48000, data)

    is_pass, anomalies, max_peak_mlsd, max_sustained_mlsd = (
        mlsd_comparator.evaluate_audio_quality(ref_path, test_path)
    )
    self.assertTrue(is_pass)
    self.assertEmpty(anomalies)
    self.assertAlmostEqual(max_peak_mlsd, 0.0, places=4)
    self.assertAlmostEqual(max_sustained_mlsd, 0.0, places=4)

  def test_evaluate_audio_quality_transient_glitch(self):
    # Create dummy sine wave
    t = np.linspace(0, 1, 48000, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t) * 10000
    ref_data = sine.astype(np.int16)
    target_data = ref_data.copy()

    # Introduce a transient glitch at ~500ms (sample 24000)
    # A tiny window of 100 samples (2.08ms)
    target_data[24000:24100] = 30000

    ref_path = self._create_dummy_wav("ref.wav", 48000, ref_data)
    test_path = self._create_dummy_wav("test.wav", 48000, target_data)

    is_pass, anomalies, _, _ = mlsd_comparator.evaluate_audio_quality(
        ref_path, test_path
    )

    self.assertFalse(is_pass)
    # Check that we caught the transient glitch
    glitches = [a for a in anomalies if a["type"] == "Transient Glitch"]
    self.assertNotEmpty(glitches)

    # It should not flag sustained degradation since it's too short
    sustained = [a for a in anomalies if a["type"] == "Sustained Degradation"]
    self.assertEmpty(sustained)

  def test_evaluate_audio_quality_sustained_degradation(self):
    # Create dummy sine wave
    t = np.linspace(0, 1, 48000, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t) * 10000
    ref_data = sine.astype(np.int16)
    target_data = ref_data.copy()

    # Introduce sustained difference at ~500ms to 700ms (200ms duration)
    target_data[24000:33600] = 0

    ref_path = self._create_dummy_wav("ref.wav", 48000, ref_data)
    test_path = self._create_dummy_wav("test.wav", 48000, target_data)

    is_pass, anomalies, _, _ = mlsd_comparator.evaluate_audio_quality(
        ref_path,
        test_path,
        sustained_threshold=1.0,
        peak_threshold=50.0,  # set very high to isolate sustained reports
        sustained_ms=100.0,
    )

    self.assertFalse(is_pass)
    sustained = [a for a in anomalies if a["type"] == "Sustained Degradation"]
    self.assertNotEmpty(sustained)
    self.assertAlmostEqual(sustained[0]["start_ms"], 500.0, delta=50.0)
    self.assertAlmostEqual(sustained[0]["end_ms"], 700.0, delta=50.0)

  def test_evaluate_audio_quality_sustained_degradation_exactly_100ms(self):
    t = np.linspace(0, 1, 48000, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t) * 10000
    ref_data = sine.astype(np.int16)
    target_data = ref_data.copy()

    # 100ms degradation (4800 samples)
    target_data[24000:28800] = 0

    ref_path = self._create_dummy_wav("ref.wav", 48000, ref_data)
    test_path = self._create_dummy_wav("test.wav", 48000, target_data)

    is_pass, anomalies, _, _ = mlsd_comparator.evaluate_audio_quality(
        ref_path,
        test_path,
        sustained_threshold=1.0,
        peak_threshold=50.0,
        sustained_ms=100.0,
    )

    self.assertFalse(is_pass)
    sustained = [a for a in anomalies if a["type"] == "Sustained Degradation"]
    self.assertNotEmpty(sustained)

  def test_evaluate_audio_quality_sustained_degradation_too_short(self):
    t = np.linspace(0, 1, 48000, endpoint=False)
    sine = np.sin(2 * np.pi * 440 * t) * 10000
    ref_data = sine.astype(np.int16)
    target_data = ref_data.copy()

    # 50ms degradation (2400 samples)
    target_data[24000:26400] = 0

    ref_path = self._create_dummy_wav("ref.wav", 48000, ref_data)
    test_path = self._create_dummy_wav("test.wav", 48000, target_data)

    is_pass, anomalies, _, _ = mlsd_comparator.evaluate_audio_quality(
        ref_path,
        test_path,
        sustained_threshold=1.0,
        peak_threshold=50.0,
        sustained_ms=100.0,
    )

    # Should pass because 50ms < 100ms target
    self.assertTrue(is_pass)
    self.assertEmpty(anomalies)

  def test_evaluate_audio_quality_consistency_check(self):
    # Verifies that a recovery region resets consecutive violation count.
    #
    # Target has two silent regions (3072 samples each) separated by a
    # 2048-sample matching region (recovery). Neither silent region is long
    # enough to trigger (requires 13 frames / 100ms), and the recovery region
    # prevents them from merging.
    #
    # Muting a pure sine tone with a rectangular window introduces boundary
    # spectral aliasing that spans the short recovery region. This elevates
    # MLSD across the recovery frames, causing independent silent regions to
    # merge into an unintended sustained failure.
    #
    # Broadband white noise avoids boundary aliasing and ensures stable metrics.
    # A local Generator isolates the RNG to avoid affecting other tests.
    rng = np.random.default_rng(42)
    ref_data = rng.normal(0, 0.5, size=12288)
    ref_data = (ref_data * 10000).astype(np.int16)

    target_data = ref_data.copy()
    target_data[0:3072] = 0
    target_data[5120:8192] = 0

    ref_path = self._create_dummy_wav("ref.wav", 48000, ref_data)
    test_path = self._create_dummy_wav("test.wav", 48000, target_data)

    is_pass, anomalies, _, max_sustained_mlsd = (
        mlsd_comparator.evaluate_audio_quality(
            ref_path,
            test_path,
            sustained_threshold=1.0,
            peak_threshold=50.0,  # Set high to ignore the mute-induced peaks
            sustained_ms=100.0,
        )
    )
    self.assertTrue(is_pass)
    self.assertLess(max_sustained_mlsd, 1.0)
    self.assertEmpty(anomalies)


if __name__ == "__main__":
  absltest.main()
