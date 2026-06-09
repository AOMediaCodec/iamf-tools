# Mel Log Spectral Distance (MLSD) Comparator

Validates audio quality by calculating Mel Log Spectral Distance (MLSD) between WAV files, averaged across all channels and frames.

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
  [--threshold=1.0] \
  [--report_file=path/to/report.txt]
```

### Arguments

*   `--ref_wav` (Required): Path to the reference (golden) WAV file.
*   `--test_wav` (Required): Path to the test WAV file to evaluate.
*   `--threshold` (Optional, default: `1.0`): The threshold for MLSD evaluation. Scores less than or equal to this value will PASS.
*   `--report_file` (Optional, default: `mlsd_report.txt`): Path where the evaluation report will be written.

## Output

### Console Output
The tool logs the average MLSD and the final status (PASS/FAIL):

```text
average MLSD: 0.2
Status: PASS
Report written to /path/to/mlsd_report.txt
```

### Report File
A text report is written to the specified `--report_file` (default: `mlsd_report.txt`) with the following format (if set to an empty string `""`, report writing is skipped):

```text
Test: MLSD evaluation
Reference: golden_reference.wav
Test: decoded_output.wav
MLSD: 0.2 (threshold: 1.0)
Result: PASS
```
