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

import os

from absl import app
from absl import flags
from absl import logging

from validation.mlsd_comparator import mlsd_comparator

_REF_WAV = flags.DEFINE_string(
    "ref_wav", None, "Path to the reference WAV file."
)
_TEST_WAV = flags.DEFINE_string("test_wav", None, "Path to the test WAV file.")
_THRESHOLD = flags.DEFINE_float(
    "threshold",
    1.0,
    "Threshold for Mel Log Spectral Distance (MLSD) evaluation.",
)
_REPORT_FILE = flags.DEFINE_string(
    "report_file", "mlsd_report.txt", "Path to write the output report."
)


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
    average_lsd = mlsd_comparator.calc_score_wav(ref_wav, test_wav)
    logging.info("average MLSD: %.1f", average_lsd)

    threshold = _THRESHOLD.value

    status = "PASS" if average_lsd <= threshold else "FAIL"
    logging.info("Status: %s", status)

    report_content = (
        "Test: MLSD evaluation\n"
        f"Reference: {ref_wav_basename}\n"
        f"Test: {test_wav_basename}\n"
        f"MLSD: {average_lsd:.1f} (threshold: {threshold})\n"
        f"Result: {status}\n"
    )

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
