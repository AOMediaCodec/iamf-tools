# IAMF Verifier (`iamf_verifier`)

The `iamf_verifier` tool checks the syntax of an encoded `.iamf` bitstream.

## Verification Pipeline

1. **Bitstream Syntax Compliance**:
   * Invokes [Compliance Warden](https://github.com/felicialim/ComplianceWarden/tree/iamf) on the test file to verify bitstream syntax compliance.

2. **Loudness Verification**:
   * Invokes `iamf_loudness_comparator_main` to verify integrated loudness and digital peak metadata align with the reference within a `+/- 0.1 LUFS` tolerance.

---

## Prerequisites

Ensure the following external executables are available on your system `PATH` or provided via explicit command-line flags:

* **[Compliance Warden](https://github.com/felicialim/ComplianceWarden/tree/iamf)** (`cw.exe`): Required for bitstream syntax verification (`--cw_cmd`). *(Note: Please use the `iamf` branch from [this fork](https://github.com/felicialim/ComplianceWarden/tree/iamf) while these IAMF compliance rules are being actively upstreamed to the main Compliance Warden repository).*
* **Internal Validation Comparators**: You must compile internal comparators like `iamf_loudness_comparator_main` via Bazelisk (e.g., `bazelisk build //validation/iamf_loudness_comparator:iamf_loudness_comparator_main`) or provide their paths via explicit command-line flags (such as `--loudness_cmd`).

---

## Usage

Execute the following instructions from the repository root:

1. Install the required Python dependencies:

   ```bash
   pip install -r validation/iamf_verifier/requirements.txt
   ```

2. Build the internal C++ comparators (using `bazelisk`):

   ```bash
   bazelisk build //validation/iamf_loudness_comparator:iamf_loudness_comparator_main
   ```

3. Run the verifier script:

   ```bash
   PYTHONPATH=. python3 validation/iamf_verifier/iamf_verifier.py \
       --ref_file=/path/to/reference.iamf \
       --test_file=/path/to/test_content.iamf \
       --cw_cmd=/path/to/ComplianceWarden/bin/cw.exe \
       --loudness_cmd=bazel-bin/validation/iamf_loudness_comparator/iamf_loudness_comparator_main \
       --report_file=iamf_verification_report.txt
   ```

### Command-Line Flags

* `--ref_file`: Path to the reference `.iamf` bitstream.
* `--test_file`: Path to the test `.iamf` bitstream.
* `--cw_cmd`: Path or invocation command for Compliance Warden.
* `--loudness_cmd`: Path to the internal loudness comparator binary. *(Note: When compiling standalone via Bazel, the binary is located at `bazel-bin/validation/iamf_loudness_comparator/iamf_loudness_comparator_main`).*
* `--report_file`: (Optional) Output text file for saving the final verification ledger (Default: `iamf_verification_report.txt`).

---

## Verification Output

The tool prints a multi-line report to `stdout` and saves it to the path specified by `--report_file`.

The report lists each verification stage and ends with an `OVERALL RESULT: PASS` (or `FAIL`):

```text
IAMF VERIFIER REPORT
====================
Reference File: reference.iamf
Test File: test_content.iamf

[PASS] Compliance Warden bitstream syntax verification

[PASS] Loudness Metadata Alignment (Tolerance +/- 0.1 LUFS)
File 1: reference.iamf
File 2: test_content.iamf
Tolerance Threshold: 0.1 LUFS

Comparing Mix Presentation ID: 42
  Sub-mix 0 Layout 0 Result: Within Tolerance
    Integrated: -30.6797 vs -30.6797 (diff: 0)
    Peak: -13.1562 vs -13.1562 (diff: 0)

Overall Result: Within Tolerance

OVERALL RESULT: PASS
```

If any individual stage detects an anomaly (e.g., bitstream syntax corruption or loudness drift exceeding the threshold), that specific stage is marked as `[FAIL]` with detailed diagnostics, and the final conclusion reflects `OVERALL RESULT: FAIL`.