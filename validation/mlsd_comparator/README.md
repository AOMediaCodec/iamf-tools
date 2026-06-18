# Mel Log Spectral Distance (MLSD) Comparator

Validates audio quality by calculating Mel Log Spectral Distance (MLSD) between WAV files.

Rather than relying on a single overall score, this tool employs a dual-criterion evaluation framework to independently detect transient glitches and long-term encoding artifacts:

* **Peak MLSD**: Detects brief transient errors (e.g., pops, clicks, or block boundary discontinuities) against a peak threshold.
* **Sustained MLSD**: Detects prolonged spectral degradation over rolling temporal windows (defined in milliseconds) against a sustained threshold.

## Prerequisites

To run this tool, you need **Python 3** installed on your system.

### Install Dependencies

Install the required Python packages using `requirements.txt`:

```bash
pip install -r /path/to/iamf-tools/validation/mlsd_comparator/requirements.txt
```

## Usage

Set `PYTHONPATH` to the repository root directory (`/path/to/iamf-tools`) so Python locates the `validation` package:

```bash
PYTHONPATH=/path/to/iamf-tools python3 /path/to/iamf-tools/validation/mlsd_comparator/mlsd_comparator_main.py \
  --ref_wav=path/to/reference.wav \
  --test_wav=path/to/test.wav \
  [--peak_threshold=0.04] \
  [--sustained_threshold=0.20] \
  [--sustained_ms=100.0] \
  [--report_file=mlsd_report.txt]
```

### Arguments

*   `--ref_wav` (Required): Path to the reference (golden) WAV file.
*   `--test_wav` (Required): Path to the test WAV file to evaluate.
*   `--peak_threshold` (Optional, default: `0.04`): Peak threshold for short transient clicks/pops.
*   `--sustained_threshold` (Optional, default: `0.20`): Sustained threshold for long-term encoding quality.
*   `--sustained_ms` (Optional, default: `100.0`): Minimum duration in milliseconds to count as a sustained degradation.
*   `--report_file` (Optional, default: `mlsd_report.txt`): Path where the evaluation report will be written.

## Output

### Console Output
The tool logs the maximum peak and sustained MLSD over all channels and windows, along with the final status (PASS/FAIL):

```text
Maximum Peak MLSD: 0.02
Maximum Sustained MLSD: 0.15
Status: PASS
Report written to /path/to/iamf-tools/mlsd_report.txt
```

### Report File
A text report is written to the specified `--report_file` (default: `mlsd_report.txt`) with the following format (if set to an empty string `""`, report writing is skipped):

```text
Test: MLSD evaluation
Reference: golden_reference.wav
Test: decoded_output.wav
Maximum Peak MLSD: 0.02 (Threshold: 0.04)
Maximum Sustained MLSD: 0.15 (Threshold: 0.20 over 100.0 ms)
Result: PASS
```
