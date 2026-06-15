# Opus Ambisonics Verifier (`opus_hoa_main`)

Checks if Opus Ambisonics Audio Elements in an IAMF bitstream (`.iamf`) follow recommendations for coding mode.

Audio Elements are classified as:

* **CANONICAL**: Matches recommended coding mode.
* **CUSTOM**: Valid, but uses non-standard coding mode.
* **INVALID OR NON-OPUS**: Corrupt, missing descriptors, or not Opus Ambisonics.

## Recommended Practices

The verifier checks if Opus Ambisonics Audio Elements adhere to recommended practices:

* **0OA to 2OA**: Use **MONO** mode (0).
* **3OA to 4OA**: Use **PROJECTION** mode (1).

## Build

```bash
bazel build //validation/opus_hoa:opus_hoa_main
```

## Run

```bash
bazel run //validation/opus_hoa:opus_hoa_main -- \
  --input=/path/to/input.iamf \
  [--report_file=opus_hoa_report.txt]
```

### Flags

* `--input` (Required): Input `.iamf` file.
* `--report_file` (Optional, default: `opus_hoa_report.txt`): Output report file.

## Output

### Console

```text
Result: All Canonical
Detailed report saved to: opus_hoa_report.txt
```

### Report File

Written to `--report_file`:

```text
[Audio Element ID: 300] Status: CANONICAL (3OA)
[Audio Element ID: 301] Status: CUSTOM (1OA)
  Rationale: Order 1 recommended practice is MONO (0) mode, but found mode: 1
[Audio Element ID: 302] Status: INVALID OR NON-OPUS (skipping)
  Details: Not an Opus Codec Config

Result: Non-Canonical Elements Detected
```
