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
import re
import shutil
import subprocess
import sys
import tempfile
import wave

from absl import app
from absl import flags
from absl import logging

from validation.mlsd_comparator import mlsd_comparator


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
    "Path to test file (.iamf or .mp4 containing IAMF).",
)
_CW_CMD = flags.DEFINE_string(
    "cw_cmd",
    None,
    "Command name or path for Compliance Warden.",
)
_DECODER_CMD = flags.DEFINE_string(
    "decoder_cmd",
    None,
    "Path to `iamf_tools` decoder binary.",
)
_LOUDNESS_CMD = flags.DEFINE_string(
    "loudness_cmd",
    None,
    "Path to loudness comparator binary.",
)
_GPAC_CMD = flags.DEFINE_string(
    "gpac_cmd", None, "Command name or path for the `gpac` binary."
)
_REPORT_FILE = flags.DEFINE_string(
    "report_file",
    "iamf_verification_report.txt",
    "Path to save the verification report.",
)

flags.mark_flag_as_required("ref_file")
flags.mark_flag_as_required("test_file")
flags.mark_flag_as_required("cw_cmd")
flags.mark_flag_as_required("decoder_cmd")
flags.mark_flag_as_required("loudness_cmd")
flags.mark_flag_as_required("gpac_cmd")


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
    test_file: Absolute path to the test .iamf or .mp4 file.
    cw_cmd: Command name or absolute path for the Compliance Warden binary.

  Returns:
    Structured CheckResult indicating syntax compliance.
  """
  spec = "iamf_isobmff" if test_file.endswith(".mp4") else "iamf"
  res = _run_cmd([cw_cmd, "-s", spec, test_file])
  if res.returncode == 0:
    return CheckResult(
        True, f"[PASS] Compliance Warden verification (-s {spec})"
    )
  return CheckResult(
      False,
      f"[FAIL] Compliance Warden (-s {spec}) detected violations in"
      f" {test_file}:\n{res.stdout}",
  )


def run_loudness_comparison(
    ref_iamf: str,
    test_iamf: str,
    loudness_cmd: str,
    tolerance_lufs: float = 0.1,
) -> tuple[CheckResult, list[int]]:
  """Verifies integrated loudness and digital peak metadata align within tolerance.

  Args:
    ref_iamf: Absolute path to the reference .iamf file.
    test_iamf: Absolute path to the test .iamf file.
    loudness_cmd: Command name or absolute path for the loudness comparator.
    tolerance_lufs: Maximum allowed deviation threshold in LUFS.

  Returns:
    A tuple (loudness_result, discovered_mix_ids).
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
    discovered_mix_ids: list[int] = []
    if os.path.exists(report_path):
      with open(report_path) as rf:
        if content := rf.read().strip():
          details = f"\n{content}"
          discovered_mix_ids = [
              int(m)
              for m in re.findall(
                  r"Comparing Mix Presentation ID:\s*(\d+)", content
              )
          ]

    if res.returncode == 0:
      return (
          CheckResult(
              True,
              "[PASS] Loudness Metadata Alignment (Tolerance +/-"
              f" {tolerance_lufs} LUFS){details}",
          ),
          discovered_mix_ids,
      )
    return (
        CheckResult(
            False,
            f"[FAIL] Loudness metadata exceeds +/- {tolerance_lufs} LUFS"
            f" tolerance:\n{res.stdout}{details}",
        ),
        discovered_mix_ids,
    )


def get_audio_frame_count(wav_path: str) -> int:
  """Returns the total frame count of an uncompressed WAV.

  Args:
    wav_path: Absolute path to the input WAV file.

  Returns:
    The total frame count.
  """
  with wave.open(wav_path, "rb") as wf:
    return wf.getnframes()


def _decode_and_evaluate_layout(
    ref_file: str,
    test_file: str,
    layout: str,
    mix_id: int,
    decoder_cmd: str,
    temp_dir: str,
) -> CheckResult:
  """Decodes reference and test bitstreams to WAV and verifies MLSD audio quality.

  Decodes both bitstreams for a given layout and mix ID, asserts exact sample
  count equality, and evaluates objective MLSD peak and sustained anomalies.

  Args:
    ref_file: Absolute path to the reference .iamf bitstream.
    test_file: Absolute path to the test .iamf bitstream.
    layout: Immersive layout string matching standard `iamf_tools` standalone
      decoder layout specifications (e.g., '7.1.4', '2.0').
    mix_id: Target integer Mix Presentation ID to decode.
    decoder_cmd: Command name or absolute path for the iamf_tools decoder.
    temp_dir: Temporary workspace directory path for decoded WAV files.

  Returns:
    Structured CheckResult indicating sample match and MLSD evaluation.
  """
  ref_wav = os.path.join(temp_dir, f"ref_mix{mix_id}_{layout}.wav")
  test_wav = os.path.join(temp_dir, f"test_mix{mix_id}_{layout}.wav")

  for infile, outfile, label in (
      (ref_file, ref_wav, "reference"),
      (test_file, test_wav, "test"),
  ):
    res = _run_cmd([
        decoder_cmd,
        f"--input_filename={infile}",
        f"--output_filename={outfile}",
        f"--output_layout={layout}",
        f"--mix_id={mix_id}",
    ])
    if res.returncode != 0:
      return CheckResult(
          False,
          f"[FAIL] Failed to decode {label} layout {layout} (Mix"
          f" ID={mix_id}):\n{res.stdout}",
      )

  ref_frames = get_audio_frame_count(ref_wav)
  test_frames = get_audio_frame_count(test_wav)

  mix_label = f" (Mix ID={mix_id})"

  if ref_frames != test_frames:
    return CheckResult(
        False,
        f"[FAIL] Sample count mismatch for layout {layout}{mix_label}:"
        f" Reference={ref_frames} vs Test={test_frames}",
    )

  is_pass, anomalies, m_peak, m_sustained = (
      mlsd_comparator.evaluate_audio_quality(ref_wav, test_wav)
  )
  if not is_pass:
    msg = f"[FAIL] MLSD verification failed for layout {layout}{mix_label}."
    if anomalies:
      msg += f" Anomalies detected: {len(anomalies)}"
    return CheckResult(False, msg)

  return CheckResult(
      True,
      f"[PASS] Layout {layout:8s}{mix_label} | Sample Count={ref_frames}"
      f" exactly | MLSD (Peak={m_peak:.2f}, Sustained={m_sustained:.2f})",
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


def _mp4_to_iamf(
    mp4_path: str,
    temp_dir: str,
    gpac_cmd: str = "gpac",
) -> str:
  """Extracts the underlying IAMF bitstream from an MP4.

  Args:
    mp4_path: Absolute path to the input MP4 file.
    temp_dir: Path to the temporary workspace directory for extracted .iamf
      files.
    gpac_cmd: Command name or absolute file path for the `gpac` binary.

  Returns:
    The absolute path to the extracted standalone IAMF bitstream.

  Raises:
    RuntimeError: If no IAMF bitstream track can be detected or extracted.
  """
  extracted_iamf = os.path.join(temp_dir, "extracted.iamf")
  res = _run_cmd([
      gpac_cmd,
      "-i",
      mp4_path,
      "-o",
      extracted_iamf,
  ])
  if res.returncode != 0 or not os.path.exists(extracted_iamf):
    raise RuntimeError(
        f"Failed to transmux IAMF bitstream from {mp4_path}:\n{res.stderr}"
    )
  return extracted_iamf


def _run_verifier(
    ref_file: str,
    test_file: str,
    cw_cmd: str,
    decoder_cmd: str,
    loudness_cmd: str,
    gpac_cmd: str = "gpac",
    layouts: tuple[str, ...] = ("7.1.4", "2.0", "Binaural"),
    loudness_tolerance_lufs: float = 0.1,
) -> tuple[bool, str]:
  """Executes the core IAMF verification pipeline.

  Validates bitstream syntax via Compliance Warden, verifies integrated
  loudness alignment against a reference bitstream within tolerance, and
  evaluates rendering quality using MLSD across immersive layouts.

  Args:
    ref_file: Absolute path to the reference .iamf file.
    test_file: Absolute path to the test .iamf or .mp4 file.
    cw_cmd: Command name or path for Compliance Warden.
    decoder_cmd: Command name or path for the `iamf_tools` decoder.
    loudness_cmd: Command name or path for the loudness comparator.
    gpac_cmd: Command name or path for the `gpac` binary.
    layouts: Tuple of immersive loudspeaker layout strings matching standard
      `iamf_tools` standalone decoder layout specifications (e.g., '7.1.4',
      '2.0').
    loudness_tolerance_lufs: Maximum allowed deviation in LUFS.

  Returns:
    A tuple (is_pass, report_string) indicating overall verification success
    and containing the formatted multi-line verification ledger.

  Raises:
    FileNotFoundError: If any required executable is missing.
    RuntimeError: If MP4 container probing or extraction fails.
  """
  is_mp4 = test_file.endswith(".mp4")

  dep_cmds = [cw_cmd, decoder_cmd, loudness_cmd]
  if is_mp4:
    dep_cmds.append(gpac_cmd)

  for cmd in dep_cmds:
    check_dependency(cmd)

  report = _generate_report_header(ref_file, test_file)
  results: list[CheckResult] = []

  with tempfile.TemporaryDirectory() as temp_dir:
    test_iamf_file = test_file
    if is_mp4:
      test_iamf_file = _mp4_to_iamf(test_file, temp_dir, gpac_cmd)

    results.append(run_compliance_warden(test_file, cw_cmd))
    loudness_res, mix_ids = run_loudness_comparison(
        ref_file, test_iamf_file, loudness_cmd, loudness_tolerance_lufs
    )
    results.append(loudness_res)

    if not mix_ids:
      results.append(
          CheckResult(
              False,
              "[FAIL] No matching Mix Presentation IDs discovered between"
              " Reference and Test bitstreams for layout decoding and MLSD"
              " evaluation.",
          )
      )
    else:
      for mix_id in mix_ids:
        for layout in layouts:
          results.append(
              _decode_and_evaluate_layout(
                  ref_file,
                  test_iamf_file,
                  layout,
                  mix_id,
                  decoder_cmd,
                  temp_dir,
              )
          )

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
      decoder_cmd=_DECODER_CMD.value,
      loudness_cmd=_LOUDNESS_CMD.value,
      gpac_cmd=_GPAC_CMD.value,
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
