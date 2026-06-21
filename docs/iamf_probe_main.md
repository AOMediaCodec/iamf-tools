# Probe

`iamf-tools` provides a command-line probe that summarizes the contents of a
standalone IAMF sequence (`.iamf`) file without decoding any audio. It prints
the descriptor OBUs — IA Sequence Header profiles, Codec Configs, Audio
Elements, and Mix Presentations (including loudness annotations and tags) —
and can optionally walk the temporal units to report parameter-block
contents, per-substream sample totals, and the stream duration.

The probe is useful for inspecting files produced by the encoder, debugging
malformed or truncated streams, and building tooling that needs IAMF metadata
without a full decode.

The underlying C++ API is `iamf_tools::Probe()` in
[`iamf/cli/probe.h`](../iamf/cli/probe.h), which returns the same data as a
structured `ProbeReport`.

## Building the probe

See [Build instructions](build_instructions.md) to build from source, then:

```
bazel build -c opt //iamf/cli:probe_main
```

## Using the probe

The input file may be passed as a positional argument, via
`--input_filename`, or as `-` to read the stream from stdin:

```
probe_main input.iamf
probe_main --input_filename=input.iamf
cat input.iamf | probe_main -
```

Pointing the probe at an MP4 file (IAMF usually travels inside MP4), or at a
stream that starts mid-sequence, produces an error explaining the problem.

Optional flags:

-   `--format` Output format: `text` (default, a human-readable summary) or
    `json`.
-   `--scan` Temporal-unit scan mode: `counts` or `full`. `counts` walks the
    temporal units after the descriptor OBUs and reports OBU counts,
    per-substream sample totals, and the overall duration; the report size
    stays independent of the stream length, so use it for duration-only
    probes of large files. `full` additionally reports parameter-block
    contents (mix-gain animations, demixing info, recon gain) and a
    per-temporal-unit index. By default no scan runs: only descriptor OBUs
    are parsed, so probing is fast even on very large files.
-   `--duration` Shorthand for `--scan=counts`.

Example:

```
bazel-bin/iamf/cli/probe_main iamf/cli/testdata/iamf/noise_1024samp_5p1_opus.iamf
```

This prints a human-readable report of the file's descriptor OBUs to stdout
(pass `--format=json` for machine-readable output). To also compute the
duration of a large file as cheaply as possible:

```
bazel-bin/iamf/cli/probe_main \
  --input_filename=input.iamf \
  --duration
```

## Robustness

The descriptor parse is strict: the probe reports an error if the input does
not begin with a valid IAMF descriptor sequence. The temporal-unit scan is
best-effort in the spirit of the specification: OBUs that fail to parse are
counted (`audio_frame_parse_errors`, `parameter_block_parse_errors`) and
skipped, and the report's `stopped_reason` records why the scan ended (`eof`,
`truncated`, `malformed`, `next_ia_sequence`, or `scan_budget_exceeded`), so
a healthy file can be distinguished from a damaged one.
