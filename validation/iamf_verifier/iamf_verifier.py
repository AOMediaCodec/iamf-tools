# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Verifies IAMF files."""

import dataclasses
import os
import shutil
import subprocess
import sys
import tempfile

from absl import app
from absl import flags
from absl import logging


@dataclasses.dataclass(frozen=True)
class CheckResult:
  """Structured outcome of an individual verification check."""

  passed: bool
  ledger_entry: str


_REF_FILE = flags.DEFINE_string(
    "ref_file", None, "Path to reference .iamf file."
)
_TEST_FILE = flags.DEFINE_string(
    "test_file",
    None,
    "Path to test .iamf file.",
)
_CW_CMD = flags.DEFINE_string(
    "cw_cmd",
    None,
    "Command name or path for Compliance Warden.",
)
_LOUDNESS_CMD = flags.DEFINE_string(
    "loudness_cmd",
    None,
    "Path to loudness comparator binary.",
)
_REPORT_FILE = flags.DEFINE_string(
    "report_file",
    "iamf_verification_report.txt",
    "Path to save the verification report.",
)

flags.mark_flag_as_required("ref_file")
flags.mark_flag_as_required("test_file")
flags.mark_flag_as_required("cw_cmd")
flags.mark_flag_as_required("loudness_cmd")


def _run_cmd(cmd: list[str]) -> subprocess.CompletedProcess[str]:
  """Executes an external command as a subprocess and logs its execution.

  Args:
    cmd: Sequence of string arguments representing the command to execute.

  Returns:
    The completed subprocess result containing stdout and stderr.
  """
  logging.info("Executing: %s", " ".join(cmd))
  return subprocess.run(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      check=False,
  )


def check_dependency(command: str) -> None:
  """Verifies that a required external command or binary exists and is executable.

  Args:
    command: Executable command name (resolved via PATH) or absolute file path.

  Raises:
    FileNotFoundError: If the required executable is not found.
  """
  if not shutil.which(command) and not os.path.exists(command):
    raise FileNotFoundError(f"Required executable not found: {command}")


def run_compliance_warden(test_file: str, cw_cmd: str) -> CheckResult:
  """Verifies bitstream syntax compliance via Compliance Warden.

  Args:
    test_file: Absolute path to the test .iamf file.
    cw_cmd: Command name or absolute path for the Compliance Warden binary.

  Returns:
    Structured CheckResult indicating syntax compliance.
  """
  res = _run_cmd([cw_cmd, "-s", "iamf", test_file])
  if res.returncode == 0:
    return CheckResult(
        True, "[PASS] Compliance Warden bitstream syntax verification"
    )
  return CheckResult(
      False,
      "[FAIL] Compliance Warden detected syntax violations in"
      f" {test_file}:\n{res.stdout}",
  )


def run_loudness_comparison(
    ref_iamf: str,
    test_iamf: str,
    loudness_cmd: str,
    tolerance_lufs: float = 0.1,
) -> CheckResult:
  """Verifies integrated loudness and digital peak metadata align within tolerance.

  Args:
    ref_iamf: Absolute path to the reference .iamf file.
    test_iamf: Absolute path to the test .iamf file.
    loudness_cmd: Command name or absolute path for the loudness comparator.
    tolerance_lufs: Maximum allowed deviation threshold in LUFS.

  Returns:
    Structured CheckResult indicating loudness alignment.
  """
  with tempfile.TemporaryDirectory() as temp_dir:
    report_path = os.path.join(temp_dir, "loudness_report.txt")
    res = _run_cmd([
        loudness_cmd,
        "--file1",
        ref_iamf,
        "--file2",
        test_iamf,
        "--tolerance",
        str(tolerance_lufs),
        "--report_file",
        report_path,
    ])
    details = ""
    if os.path.exists(report_path):
      with open(report_path) as rf:
        if content := rf.read().strip():
          details = f"\n{content}"

    if res.returncode == 0:
      return CheckResult(
          True,
          "[PASS] Loudness Metadata Alignment (Tolerance +/-"
          f" {tolerance_lufs} LUFS){details}",
      )
    return CheckResult(
        False,
        f"[FAIL] Loudness metadata exceeds +/- {tolerance_lufs} LUFS"
        f" tolerance:\n{res.stdout}{details}",
    )


def _generate_report_header(ref_file: str, test_file: str) -> list[str]:
  """Generates standardized structured report header lines for verification ledgers.

  Args:
    ref_file: Absolute path to the reference .iamf file.
    test_file: Absolute path to the test .iamf file.

  Returns:
    A list of formatted report header strings.
  """
  return [
      "IAMF VERIFIER REPORT",
      "====================",
      f"Reference File: {os.path.basename(ref_file)}",
      f"Test File: {os.path.basename(test_file)}",
      "",
  ]


def _run_verifier(
    ref_file: str,
    test_file: str,
    cw_cmd: str,
    loudness_cmd: str,
    loudness_tolerance_lufs: float = 0.1,
) -> tuple[bool, str]:
  """Executes the core IAMF verification pipeline.

  Validates bitstream syntax via Compliance Warden and verifies integrated
  loudness alignment against a reference bitstream within the specified
  tolerance.

  Args:
    ref_file: Absolute path to the reference .iamf file.
    test_file: Absolute path to the test .iamf file.
    cw_cmd: Command name or path for Compliance Warden.
    loudness_cmd: Command name or path for the loudness comparator.
    loudness_tolerance_lufs: Maximum allowed deviation in LUFS.

  Returns:
    A tuple (is_pass, report_string) indicating overall verification success
    and containing the formatted multi-line verification ledger.

  Raises:
    FileNotFoundError: If any required executable is missing.
  """
  for cmd in (cw_cmd, loudness_cmd):
    check_dependency(cmd)

  report = _generate_report_header(ref_file, test_file)

  results = [
      run_compliance_warden(test_file, cw_cmd),
      run_loudness_comparison(
          ref_file, test_file, loudness_cmd, loudness_tolerance_lufs
      ),
  ]

  report.extend(r.ledger_entry for r in results)

  is_pass = all(r.passed for r in results)
  overall = "PASS" if is_pass else "FAIL"
  report.extend(["", f"OVERALL RESULT: {overall}"])
  return is_pass, "\n".join(report) + "\n"


def main(argv: list[str]) -> None:
  if len(argv) > 1:
    raise app.UsageError("Unexpected positional arguments.")

  is_pass, report = _run_verifier(
      ref_file=_REF_FILE.value,
      test_file=_TEST_FILE.value,
      cw_cmd=_CW_CMD.value,
      loudness_cmd=_LOUDNESS_CMD.value,
  )

  print(report)

  if report_path := _REPORT_FILE.value:
    try:
      with open(report_path, "w") as rf:
        rf.write(report)
      logging.info("Verification report written to: %s", report_path)
    except OSError as err:
      logging.warning("Could not write report to %s: %s", report_path, err)

  if not is_pass:
    sys.exit(1)


if __name__ == "__main__":
  app.run(main)
