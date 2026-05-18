# eval/ — the autoslam harness

This directory is the **frozen contract** between the agent and the benchmark. The agent reads this README at every session start; it never modifies anything in `eval/`.

## Files

- `run_experiment.sh` — entry point. Builds the experiment, runs it on dev or full sequences, validates output, scores via vendored devkit, appends a row to `results.tsv`.
- `_harness.py` — the orchestration (Python 3, stdlib only).
- `devkit/evaluate_odometry.cpp` — vendored KITTI evaluator (C++17, single file, self-contained). Produces JSON-per-line on stdout.
- `devkit/build.sh` — idempotent build for the devkit. Called automatically by the harness on first run.
- `devkit/CMakeLists.txt` — optional CMake (not used by default; build.sh just invokes g++).

## Invocation

```bash
# Dev set {00, 05, 07}, per-sequence cap 10 min, RAM cap 8 GB
bash eval/run_experiment.sh experiments/expNNNN_<slug>

# Full set {00..10}, per-sequence cap 30 min
bash eval/run_experiment.sh experiments/expNNNN_<slug> --full
```

The agent invokes this once per experiment (dev). If the new dev result beats the current best by ≥ 0.05% trans, the agent invokes it again with `--full` to promote.

## What the harness does

1. **Build devkit** (`bash eval/devkit/build.sh`) if not already built.
2. **Build experiment** (`bash experiments/expNNNN_*/build.sh`). On failure: log to `run_logs/EXP_build.log`, write a `build_fail` row, exit.
3. **For each sequence** (in parallel, up to ⌊N_cores / `meta.yaml.threads`⌋ slots):
   - Materialize a per-sequence config by substituting placeholders into `config_template.{yaml,yml,toml,json}`.
   - Invoke `./slam <config>` under `taskset` (CPU pinning) + `ulimit -v` (8 GB RAM) + `timeout` (10 or 30 min).
   - Validate output: line count must match `times.txt`, every line must be 12 finite floats.
   - On crash / timeout / OOM / invalid output: write a penalty trajectory (stays at origin = ~100% trans error), mark sequence as failed.
4. **Evaluate** (`devkit/evaluate_odometry <gt_dir> <pred_dir> <seqs...>`) — yields per-sequence + aggregate trans/rot errors as JSON.
5. **Determinism check** (dev keep-candidates only): re-run dev sequences, require bit-exact identical pose files. Mismatch → `flaky`, reject.
6. **Append/update results row** in `results.tsv` with status: `keep | reject | flaky | partial | build_fail`.

## Per-experiment contract (what `_harness.py` requires)

The experiment dir must contain:
- `build.sh` — exits 0, leaves `./slam` next to itself.
- `config_template.{yaml,yml,toml,json}` — references `{sequence_dir}`, `{output_path}`, `{timestamps_path}`, `{calib_path}`, optionally `{time_budget_s}`.
- `meta.yaml` — at minimum `parent`, `description`, `language`, `threads`, `deterministic: true`.

The `./slam` binary must:
- Accept `./slam <config_path>` (single arg).
- Write the trajectory file referenced by the config's `output_path` placeholder: N lines × 12 floats (row-major 3×4, camera frame, KITTI convention).
- Exit 0 on success.

See `experiments/exp0000_identity/` for a minimal worked example.

## Scoring details

- **Metric**: official KITTI translation error %, computed over sub-trajectories of {100, 200, ..., 800} m with start-stride 10 frames. Lower is better.
- **Aggregation**: sum of per-sub-trajectory errors / count, across all sequences in the set. Matches the leaderboard's formula.
- **Penalty for failed sequences**: a stay-at-origin trajectory contributes ~100% trans error to the aggregate. Dev with any failed seq → `partial`, not keep-eligible.
- **Promotion**: handled by the agent (program.md §4), not by the harness. The harness just runs whatever set you ask for.

## Logs

- `run_logs/EXP_build.log` — build output.
- `run_logs/EXP/SEQ.log` — per-sequence stdout + stderr.
- `run_logs/EXP/SEQ.time` — `/usr/bin/time -v` output (peak RSS lives here).
- `run_logs/last_eval.log` — last devkit invocation.

## Constraints the harness enforces

| Resource | Cap | Mechanism |
|---|---|---|
| Per-seq wall (dev) | 10 min | `timeout 600` |
| Per-seq wall (full) | 30 min | `timeout 1800` |
| Per-process RAM | 8 GB | `ulimit -v 8388608` |
| CPU pinning | `meta.yaml.threads` cores | `taskset -c` |
| Parallel slots | ⌊cores / threads⌋ | thread pool |
| Determinism | bit-exact pose file | re-run + byte-compare |

## Smoke test

```bash
bash eval/run_experiment.sh experiments/exp0000_identity
```

Expected: dev_trans_pct ≈ 100, status = `keep` (it's the first row, so it sets the "best" floor), all three dev sequences complete with status `ok`.
