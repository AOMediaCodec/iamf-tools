# Opus Ambisonics Verifier (`opus_hoa_main`)

Checks if Opus Ambisonics Audio Elements in an IAMF bitstream (`.iamf`) follow recommendations for coding mode and demixing matrix.

Audio Elements are classified as:

* **CANONICAL**: Matches recommended mode and matrix.
* **CUSTOM**: Valid, but uses non-standard mode or matrix.
* **INVALID OR NON-OPUS**: Corrupt, missing descriptors, or not Opus Ambisonics.

## Recommended Practices

The verifier checks if Opus Ambisonics Audio Elements adhere to recommended practices:

* **0OA to 2OA**: Use **MONO** mode (0).
* **3OA to 4OA**: Use **PROJECTION** mode (1) with the reference Opus demixing matrix coefficients.
* **PROJECTION mode**: `coupled_substream_count` should be the floor of half the total input channel count.

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
[Audio Element ID: 302] Status: CUSTOM (3OA)
  Rationale: Demixing matrix coefficients diverge from Opus Channel Mapping Family 3 reference.
[Audio Element ID: 303] Status: CUSTOM (3OA)
  Rationale: coupled_substream_count should be the floor of half the total input channel count.
[Audio Element ID: 304] Status: INVALID OR NON-OPUS (skipping)
  Details: Not an Opus Codec Config

Result: Non-Canonical Elements Detected
```
