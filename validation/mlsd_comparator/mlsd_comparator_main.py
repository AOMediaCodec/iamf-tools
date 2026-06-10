# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Binary for Mel Log Spectral Distance (MLSD) comparator."""

import collections
import os

from absl import app
from absl import flags
from absl import logging

from validation.mlsd_comparator import mlsd_comparator

_REF_WAV = flags.DEFINE_string(
    "ref_wav", None, "Path to the reference WAV file."
)
_TEST_WAV = flags.DEFINE_string("test_wav", None, "Path to the test WAV file.")
_PEAK_THRESHOLD = flags.DEFINE_float(
    "peak_threshold",
    0.04,
    "Peak threshold for Mel Log Spectral Distance (MLSD) evaluation.",
)
_SUSTAINED_THRESHOLD = flags.DEFINE_float(
    "sustained_threshold",
    0.20,
    "Sustained threshold for Mel Log Spectral Distance (MLSD) evaluation.",
)
_SUSTAINED_MS = flags.DEFINE_float(
    "sustained_ms",
    100.0,
    "Minimum duration in milliseconds to count as a sustained degradation.",
)
_REPORT_FILE = flags.DEFINE_string(
    "report_file", "mlsd_report.txt", "Path to write the output report."
)


def _format_anomalies(anomalies):
  """Formats anomalies into human-readable summary lines.

  Example outputs:
    "- Sustained Degradation (Channel 0): 500.0ms to 700.0ms (Avg MLSD: 4.20)"
    "- 1 Transient Glitch (Channel 0): at 500.0ms (MLSD: 4.20)"
    "- 47 Transient Glitches (Channel 0): clustered from 500.0ms to 550.0ms (Max
  MLSD: 4.20)"

  Args:
    anomalies: A list of anomaly dictionaries detected during evaluation.

  Returns:
    A list of formatted string summary lines describing the anomalies.
  """
  formatted = []

  for a in anomalies:
    if a["type"] == "Sustained Degradation":
      formatted.append(
          f"- Sustained Degradation (Channel {a['channel']}): "
          f"{a['start_ms']:.1f}ms to {a['end_ms']:.1f}ms (Avg MLSD:"
          f" {a['avg_mlsd']:.2f})"
      )

  # Group transient glitches per channel
  ch_glitches = collections.defaultdict(list)
  for a in anomalies:
    if a["type"] == "Transient Glitch":
      ch_glitches[a["channel"]].append(a)

  for ch, glitches in sorted(ch_glitches.items()):
    count = len(glitches)
    max_mlsd = max(g["mlsd"] for g in glitches)
    if count == 1:
      formatted.append(
          f"- 1 Transient Glitch (Channel {ch}): at"
          f" {glitches[0]['time_ms']:.1f}ms (MLSD: {max_mlsd:.2f})"
      )
    else:
      start_ms = min(g["time_ms"] for g in glitches)
      end_ms = max(g["time_ms"] for g in glitches)
      formatted.append(
          f"- {count} Transient Glitches (Channel {ch}): clustered from"
          f" {start_ms:.1f}ms to {end_ms:.1f}ms (Max MLSD: {max_mlsd:.2f})"
      )

  return formatted


def main(argv):
  """Main function for Mel Log Spectral Distance calculation script."""
  if len(argv) > 1:
    raise app.UsageError("Unexpected positional arguments.")

  if not _REF_WAV.value:
    raise app.UsageError("Flag --ref_wav must have a value other than None.")
  if not _TEST_WAV.value:
    raise app.UsageError("Flag --test_wav must have a value other than None.")

  ref_wav = _REF_WAV.value
  test_wav = _TEST_WAV.value
  report_file_path = _REPORT_FILE.value

  ref_wav_basename = os.path.basename(ref_wav)
  test_wav_basename = os.path.basename(test_wav)

  try:
    is_pass, anomalies, max_peak, max_sustained = (
        mlsd_comparator.evaluate_audio_quality(
            ref_wav,
            test_wav,
            sustained_threshold=_SUSTAINED_THRESHOLD.value,
            peak_threshold=_PEAK_THRESHOLD.value,
            sustained_ms=_SUSTAINED_MS.value,
        )
    )
    logging.info("Maximum Peak MLSD: %.2f", max_peak)
    logging.info("Maximum Sustained MLSD: %.2f", max_sustained)

    status = "PASS" if is_pass else "FAIL"
    logging.info("Status: %s", status)

    report_lines = [
        "Test: MLSD evaluation",
        f"Reference: {ref_wav_basename}",
        f"Test: {test_wav_basename}",
        (
            f"Maximum Peak MLSD: {max_peak:.2f} (Threshold:"
            f" {_PEAK_THRESHOLD.value:.2f})"
        ),
        (
            f"Maximum Sustained MLSD: {max_sustained:.2f} (Threshold:"
            f" {_SUSTAINED_THRESHOLD.value:.2f} over"
            f" {_SUSTAINED_MS.value:.1f} ms)"
        ),
        f"Result: {status}",
    ]

    if anomalies:
      report_lines.append("\nDetected Anomalies:")
      formatted_anomalies = _format_anomalies(anomalies)
      for line in formatted_anomalies:
        report_lines.append(line)
        logging.info("%s", line)

    report_content = "\n".join(report_lines) + "\n"

  except ValueError as err:
    logging.error("%s", err)
    report_content = (
        "Test: MLSD evaluation\n"
        f"Reference: {ref_wav_basename}\n"
        f"Test: {test_wav_basename}\n"
        "Result: ERROR\n"
        f"Error: {str(err)}\n"
    )

  if report_file_path:
    try:
      with open(report_file_path, "w") as report_file:
        report_file.write(report_content)
      logging.info("Report written to %s", os.path.abspath(report_file_path))
    except OSError as err:
      logging.warning(
          "Could not write report file to %s: %s", report_file_path, err
      )


if __name__ == "__main__":
  app.run(main)
