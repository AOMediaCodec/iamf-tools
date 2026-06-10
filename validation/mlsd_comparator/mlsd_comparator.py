# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Tools for Mel Log Spectral Distance (MLSD) audio quality evaluation.

This module provides functions to calculate MLSD between audio signals and
evaluate audio quality using a dual-criterion framework:
- Peak (Transient) Rule: Detects short-duration, highly audible glitches (e.g.,
  clicks or pops) when any single frame exceeds a high peak threshold.
- Sustained Rule: Detects longer-term degradations (e.g., compression artifacts)
  when a consecutive block of frames (e.g., 100 ms) remains above a lower
  sustained threshold.
"""

import os
from typing import Any
import wave

import librosa
import numpy as np
from scipy.io import wavfile

_N_FFT = 2048
_HOP_LENGTH = 512


def calc_per_channel_mlsd_pcm(
    ref_signal: np.ndarray,
    signal: np.ndarray,
    sampling_rate: int,
) -> list[np.ndarray]:
  """Calculates the Mel log spectral distance between two signals per frame.

  Args:
    ref_signal: The reference signal as a numpy array.
    signal: The signal to compare as a numpy array.
    sampling_rate: The sampling rate of the signals in Hz.

  Returns:
    A list of numpy arrays, where each array contains the Mel log spectral
    distance in dB per frame for a channel.
  """
  eps = 1e-4

  # Convert to float
  ref_signal = ref_signal / np.iinfo(ref_signal.dtype).max
  signal = signal / np.iinfo(signal.dtype).max

  mlsd_list = list()

  # To support mono channel
  num_channels = 1 if not ref_signal.shape[1:] else ref_signal.shape[1]
  for i in range(num_channels):
    ref_channel = ref_signal[:, i] if num_channels > 1 else ref_signal
    signal_channel = signal[:, i] if num_channels > 1 else signal

    # Compute mel spectrogram
    mel_ref = librosa.feature.melspectrogram(
        y=ref_channel, sr=sampling_rate, n_fft=_N_FFT, hop_length=_HOP_LENGTH
    )
    mel_signal = librosa.feature.melspectrogram(
        y=signal_channel, sr=sampling_rate, n_fft=_N_FFT, hop_length=_HOP_LENGTH
    )

    log_mel_ref = 10 * np.log10(mel_ref + eps)
    log_mel_signal = 10 * np.log10(mel_signal + eps)

    diff_squared = (log_mel_ref - log_mel_signal) ** 2

    # Average across mel bins, which is the 0th dimension
    mlsd_per_frame = np.sqrt(np.mean(diff_squared, axis=0))

    # shape: (1, num_frames) -> (num_frames,)
    mlsd_per_frame = np.squeeze(mlsd_per_frame)

    mlsd_list.append(mlsd_per_frame)

  return mlsd_list


def detect_transient_glitches(
    channel_mlsd: np.ndarray,
    channel_index: int,
    peak_threshold: float,
    hop_duration_ms: float,
) -> tuple[list[dict[str, Any]], float]:
  """Detects transient glitches and returns them along with maximum peak MLSD.

  Args:
    channel_mlsd: Numpy array containing the MLSD per frame for a channel.
    channel_index: The channel index.
    peak_threshold: Threshold for transient glitches.
    hop_duration_ms: The time step between consecutive frames in milliseconds.

  Returns:
    A tuple of:
      - anomalies: A list of dicts describing each detected transient glitch.
      - max_peak_mlsd: The maximum single-frame MLSD observed for this
        channel.
  """
  max_peak = float(np.max(channel_mlsd)) if len(channel_mlsd) > 0 else 0.0
  anomalies = []

  violating_indices = np.where(channel_mlsd > peak_threshold)[0]
  for idx in violating_indices:
    time_ms = float(idx * hop_duration_ms)
    anomalies.append({
        "type": "Transient Glitch",
        "channel": channel_index,
        "frame_index": int(idx),
        "time_ms": time_ms,
        "mlsd": float(channel_mlsd[idx]),
    })

  return anomalies, max_peak


def detect_sustained_degradations(
    channel_mlsd: np.ndarray,
    channel_index: int,
    sustained_threshold: float,
    hop_duration_ms: float,
    sustained_ms: float,
) -> tuple[list[dict[str, Any]], float]:
  """Detects sustained degradations using rolling consecutive block analysis.

  Args:
    channel_mlsd: Numpy array containing the MLSD per frame for a channel.
    channel_index: The channel index.
    sustained_threshold: Threshold for sustained degradations.
    hop_duration_ms: The time step between consecutive frames in milliseconds.
    sustained_ms: Minimum duration in milliseconds to count as sustained.

  Returns:
    A tuple of:
      - anomalies: A list of dicts describing each sustained degradation block.
      - max_sustained_mlsd: The maximum rolling minimum MLSD over any
        window of size sustained_ms observed for this channel.
  """
  anomalies = []
  if len(channel_mlsd) == 0:
    return [], 0.0

  min_consecutive_frames = max(1, int(np.ceil(sustained_ms / hop_duration_ms)))

  # Calculate maximum rolling minimum
  if len(channel_mlsd) >= min_consecutive_frames:
    rolling_mins = [
        np.min(channel_mlsd[i : i + min_consecutive_frames])
        for i in range(len(channel_mlsd) - min_consecutive_frames + 1)
    ]
    max_sustained = float(np.max(rolling_mins))
  else:
    max_sustained = float(np.min(channel_mlsd))

  # Detect blocks
  is_above = channel_mlsd > sustained_threshold
  block_start = None

  for i, above in enumerate(is_above):
    if above:
      if block_start is None:
        block_start = i
    else:
      if block_start is not None:
        duration_frames = i - block_start
        duration_time_ms = duration_frames * hop_duration_ms
        if (
            duration_time_ms >= sustained_ms
            or duration_frames >= min_consecutive_frames
        ):
          anomalies.append({
              "type": "Sustained Degradation",
              "channel": channel_index,
              "start_frame": block_start,
              "end_frame": i - 1,
              "start_ms": float(block_start * hop_duration_ms),
              "end_ms": float(block_start * hop_duration_ms + duration_time_ms),
              "duration_ms": float(duration_time_ms),
              "avg_mlsd": float(np.mean(channel_mlsd[block_start:i])),
          })
        block_start = None

  if block_start is not None:
    duration_frames = len(channel_mlsd) - block_start
    duration_time_ms = duration_frames * hop_duration_ms
    if (
        duration_time_ms >= sustained_ms
        or duration_frames >= min_consecutive_frames
    ):
      anomalies.append({
          "type": "Sustained Degradation",
          "channel": channel_index,
          "start_frame": block_start,
          "end_frame": len(channel_mlsd) - 1,
          "start_ms": float(block_start * hop_duration_ms),
          "end_ms": float(block_start * hop_duration_ms + duration_time_ms),
          "duration_ms": float(duration_time_ms),
          "avg_mlsd": float(np.mean(channel_mlsd[block_start:])),
      })

  return anomalies, max_sustained


def evaluate_audio_quality(
    ref_filepath: str,
    target_filepath: str,
    peak_threshold: float = 0.04,
    sustained_threshold: float = 0.20,
    sustained_ms: float = 100.0,
) -> tuple[bool, list[dict[str, Any]], float, float]:
  """Evaluates audio quality using dual-criterion peak and sustained thresholds.

  Args:
    ref_filepath: Path to the reference WAV file.
    target_filepath: Path to the target WAV file to compare.
    peak_threshold: Higher threshold for short transient clicks/pops.
    sustained_threshold: Lower threshold for long-term encoding quality.
    sustained_ms: Minimum duration in milliseconds to count as sustained.

  Returns:
    A tuple of:
      - is_pass: Boolean indicating if the files pass the evaluation.
      - anomalies: A list of dicts describing detected anomalies.
      - max_peak_mlsd: The overall maximum single-frame MLSD.
      - max_sustained_mlsd: The overall maximum rolling minimum MLSD.

  Raises:
    ValueError: If the wav files have different samplerate, channels, bit-depth
      or number of samples.
  """
  ref_wav = wave.open(ref_filepath, "rb")
  target_wav = wave.open(target_filepath, "rb")

  sampling_rate = ref_wav.getframerate()

  # Check sampling rate
  if sampling_rate != target_wav.getframerate():
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

  mlsd_list = calc_per_channel_mlsd_pcm(ref_data, target_data, sampling_rate)

  hop_duration_ms = (_HOP_LENGTH / sampling_rate) * 1000.0

  max_peak_mlsd = 0.0
  max_sustained_mlsd = 0.0
  anomalies = []

  for ch_idx, ch_mlsd in enumerate(mlsd_list):
    ch_glitches, ch_max_peak = detect_transient_glitches(
        ch_mlsd, ch_idx, peak_threshold, hop_duration_ms
    )
    max_peak_mlsd = max(max_peak_mlsd, ch_max_peak)
    anomalies.extend(ch_glitches)

    ch_sustained, ch_max_sust = detect_sustained_degradations(
        ch_mlsd, ch_idx, sustained_threshold, hop_duration_ms, sustained_ms
    )
    max_sustained_mlsd = max(max_sustained_mlsd, ch_max_sust)
    anomalies.extend(ch_sustained)

  is_pass = len(anomalies) == 0
  return is_pass, anomalies, max_peak_mlsd, max_sustained_mlsd
