# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0025_kitti-scan-fix-v2** — dev 0.6447%, **full 0.7748%**, rot 0.0030 deg/m.
- Per-seq full: `00=0.72 01=1.04 02=0.70 03=0.88 04=0.42 05=0.52 06=0.35 07=0.55 08=1.03 09=1.04 10=0.91`.
- Now ranked between leaderboard's #15 KISS-ICP (0.61%) and #20 filter-reg (0.65%) — equivalent to mid-leaderboard territory.

## Trajectory after 26 experiments
```
exp0000_identity         60.41%
exp0001_naive-p2p-icp    25.23%
exp0002_const-vel-init   21.24%
exp0003_kdtree-voxel1m    7.99%
exp0004_frame-to-map-k5   3.57%
exp0005_maxdist-schedule  3.03%
exp0006_window-k10        2.15%
exp0007_window-k20        1.71%
exp0008_src-voxel-05      1.06%
exp0009_trimmed-icp-80    0.97%
exp0011_trim-motion-aware 0.96%
exp0020_trim-95           0.88%
exp0022_src-voxel-04      0.87%
exp0025_kitti-scan-fix-v2 0.77%  <- current best
```

## Critical methodology lesson from exp0025

**Dev set (00, 05, 07) is urban-biased and CANNOT detect changes that benefit highway/long-distance sequences.** exp0025 rejected on dev (0.6447 vs 0.6346) but ACTUALLY DOMINATED on full (0.7748 vs 0.8729). The user correctly pushed back when I trusted the dev-reject signal alone.

When a change has strong literature support but ambiguous dev results, force a `--full` evaluation as a diagnostic. Specifically applies to:
- Sensor-intrinsic corrections that affect all data uniformly
- Changes targeting failure modes (highway, long drift) not present in dev
- Anything inspired by published methods that report different metrics

## Last single experiment (exp0026)

Tried stacking scan correction with source voxel 0.4m. Result: full 0.804% — small REGRESSION vs exp0025 (0.775%). The two changes do NOT compose additively. 10 of 11 sequences slightly regressed.

Possible: with corrected geometry, the finer source picks up noise that was previously masked. Or non-linear interaction in ICP convergence.

## Direction for next sessions

1. **Investigate seq 09 regression in exp0025** (the only sequence the scan correction hurt: 0.66 → 1.04). Hypothesis: 09's specific motion pattern interacts with the bias differently.
2. **Smaller tuning on exp0025 base**: MAX_DIST schedule, trim fractions, K window.
3. **LOAM feature classification**: ranks #3 on leaderboard at 0.55%; the path to sub-0.7%.
4. **Voxel grid local map** (KISS-ICP-style with bounded N_max/voxel): structural change to unlock further KISS-ICP-style improvements.

## What's working (don't lose)
- Const-vel init, K=20 sliding window, kd-tree NN with k-NN, voxel 1.0m map / **0.5m source** (NOT 0.4m on top of correction), MAX_DIST 3.0/1.5, motion-aware gentle trim (95%), **KITTI per-point +0.205° vertical-angle correction**, deterministic single-threaded.
- 26 experiments, all builds clean.
- Wall full-eval ~630s.

## Open questions
- Why does the KITTI scan correction help broadly but hurt seq 09 (0.66 → 1.04)?
- Why does combining two independently-positive changes (scan correction + 0.4m source) regress? Stack interactions.
- LOAM ranks 0.55% with edge/plane feature classification — is that path worth ~200 LOC of new feature extraction code?
- 01 highway after scan correction: 1.04% — down from 2.0% pre-correction. Still the highest-error sequence. Frame-to-map ceiling?
