# Opus Ambisonics Verifier (`opus_hoa_main`)

In-progress utility to inspect Opus Ambisonics Audio Elements in an IAMF bitstream (`.iamf`). Specifically, this initial phase implements bitstream ingestion and OBU descriptor parsing.

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

Currently, the skeleton parses the bitstream and writes a placeholder report to `--report_file`:

```text
Result: No Opus Ambisonics Audio Elements found.
```
