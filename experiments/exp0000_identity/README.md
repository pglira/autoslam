# exp0000_identity

The minimum viable contract reference. This experiment is part of the harness, not part of the research. **Do not extend this; do not fork from it.** Read it once to understand the contract, then fork from whatever real SLAM is current best in `results.tsv`.

## What it does

Reads the config file, counts lines in `times.txt`, writes that many identity 3×4 matrices to the output path. Pure I/O, no algorithm.

## Why it exists

To prove (and exemplify) that the harness can:
1. Build an experiment from `build.sh`.
2. Substitute placeholders into `config_template.yaml`.
3. Invoke `./slam <config_path>`.
4. Read the resulting trajectory and run it through the official KITTI evaluator.
5. Append a valid row to `results.tsv`.

When the smoke test runs (`bash eval/run_experiment.sh experiments/exp0000_identity`), this experiment should complete every dev sequence, produce a valid trajectory, and score `dev_trans_pct ≈ 100` (yes, one hundred — identity poses don't move).

## Contract details exemplified here

- `build.sh` — single-line `cc` invocation, leaves `./slam` next to the script.
- `config_template.yaml` — minimal: just the four required placeholders + `time_budget_s` (which this binary ignores).
- `meta.yaml` — `parent: null`, `deterministic: true`, no deps.
- `slam.c` — a 100-line C file that parses minimal YAML, counts lines, writes pose lines.

Your real experiments will be larger but follow the same shape.
