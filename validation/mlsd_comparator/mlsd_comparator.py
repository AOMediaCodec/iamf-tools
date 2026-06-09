# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Calculates the Mel Log Spectral Distance (MLSD) between two WAV files."""

import os
import wave

import librosa
import numpy as np
from scipy.io import wavfile


def calc_per_channel_lsd_pcm(
    ref_signal: np.ndarray, signal: np.ndarray, sampling_rate: int
):
  """Calculates the log spectral distance using Mel bins between two signals.

  Args:
    ref_signal: The reference signal as a numpy array.
    signal: The signal to compare as a numpy array.
    sampling_rate: The sampling rate of the signals in Hz.

  Returns:
    The per channel log spectral distance in dB.
  """
  eps = 1e-4

  # Convert to float
  ref_signal = ref_signal / np.iinfo(ref_signal.dtype).max
  signal = signal / np.iinfo(signal.dtype).max

  lsd_list = list()

  # To support mono channel
  num_channels = 1 if not ref_signal.shape[1:] else ref_signal.shape[1]
  for i in range(num_channels):
    ref_channel = ref_signal[:, i] if num_channels > 1 else ref_signal
    signal_channel = signal[:, i] if num_channels > 1 else signal

    # Compute mel spectrogram
    mel_ref = librosa.feature.melspectrogram(y=ref_channel, sr=sampling_rate)
    mel_signal = librosa.feature.melspectrogram(
        y=signal_channel, sr=sampling_rate
    )

    log_mel_ref = 10 * np.log10(mel_ref + eps)
    log_mel_signal = 10 * np.log10(mel_signal + eps)

    diff_squared = (log_mel_ref - log_mel_signal) ** 2

    # Average across mel bins, which is the 0th dimension
    lsd_per_frame = np.sqrt(np.mean(diff_squared, axis=0))

    # shape: (1, num_frames) -> (num_frames,)
    lsd_per_frame = np.squeeze(lsd_per_frame)

    lsd_value = np.mean(lsd_per_frame)
    lsd_list.append(lsd_value)

  return lsd_list


def calc_score_wav(ref_filepath: str, target_filepath: str):
  """Calculates the Mel log spectral distance between two WAV files.

  Args:
    ref_filepath: Path to the reference WAV file.
    target_filepath: Path to the target WAV file to compare.

  Returns:
    The Mel log spectral distance in dB, averaged over all channels.

  Raises:
    Exception: If the wav files have different samplerate, channels, bit-depth
      or number of samples.
  """
  ref_wav = wave.open(ref_filepath, "rb")
  target_wav = wave.open(target_filepath, "rb")

  # Check sampling rate
  if ref_wav.getframerate() != target_wav.getframerate():
    raise ValueError(
        "Sampling rate of reference file and comparison file are different:"
        f" {os.path.basename(ref_filepath)} vs"
        f" {os.path.basename(target_filepath)}"
    )

  # Check number of channels
  if ref_wav.getnchannels() != target_wav.getnchannels():
    raise ValueError(
        "Number of channels of reference file and comparison file are"
        f" different: {os.path.basename(ref_filepath)} vs"
        f" {os.path.basename(target_filepath)}"
    )

  # Check number of samples
  if ref_wav.getnframes() != target_wav.getnframes():
    raise ValueError(
        "Number of samples of reference file and comparison file are"
        f" different: {os.path.basename(ref_filepath)} vs"
        f" {os.path.basename(target_filepath)}"
    )

  # Check bit depth
  if ref_wav.getsampwidth() != target_wav.getsampwidth():
    raise ValueError(
        "Bit depth of reference file and comparison file are different:"
        f" {os.path.basename(ref_filepath)} vs"
        f" {os.path.basename(target_filepath)}"
    )

  # Open wav as a np array
  _, ref_data = wavfile.read(ref_filepath)
  _, target_data = wavfile.read(target_filepath)

  scores_list = calc_per_channel_lsd_pcm(
      ref_data, target_data, ref_wav.getframerate()
  )

  return np.mean(scores_list)
