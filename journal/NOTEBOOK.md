# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0009_trimmed-icp-80** — dev 0.71%, **full 0.97%**, rot 0.0037 deg/m.
- Per-seq full: `00=0.76 01=2.14 02=1.17 03=0.97 04=0.88 05=0.67 06=0.61 07=0.45 08=1.00 09=0.85 10=1.23`.
- 10/11 sequences sub-1.5%, 7/11 sub-1%. Only highway (01) remains above 2%.

## Trajectory so far (9 experiments)
```
exp0000_identity         60.41%   (baseline)
exp0001_naive-p2p-icp    25.23%   (-35.18)
exp0002_const-vel-init   21.24%   (-3.99)
exp0003_kdtree-voxel1m    7.99%   (-13.25)
exp0004_frame-to-map-k5   3.57%   (-4.42)
exp0005_maxdist-schedule  3.03%   (-0.54)
exp0006_window-k10        2.15%   (-0.88)  ← reflection 0001
exp0007_window-k20        1.71%   (-0.44)
exp0008_src-voxel-05      1.06%   (-0.65)
exp0009_trimmed-icp-80    0.97%   (-0.09)
```

KISS-ICP leaderboard sits around 0.5% full-set; we're roughly 2× away from that.

## Last delta (exp0008 → exp0009, trimmed ICP)
- Aggregate dropped slightly (1.06 → 0.97) but per-seq is mixed for the first time:
  - 07 nice: 0.82 → 0.45 (urban, lots of clean geometry, trimming removes the noisy tail)
  - 05 nice: 0.79 → 0.67
  - 10 regressed: 0.76 → 1.23 (residential — trimming may have dropped legit parked-vehicle features)
  - 01 regressed: 1.90 → 2.14 (highway sparse features; trimming removes good info, not noise)
- Lesson: **rank-based trimming is sequence-character-dependent**. Helps dense urban; hurts sparse/open-feature scenes. A future experiment could make TRIM_KEEP_FRAC adaptive — e.g., keep more when n_corrs is low to begin with.

## Current direction
01 (highway, 2.14%) is now the sharp single-sequence outlier. Most other sequences are sub-1% or right at it. Three main directions for next block:

1. **Adaptive trim**: per-frame keep-fraction based on correspondence count. Low count → keep more (don't waste rare signals).
2. **Point-to-plane ICP**: the next big algorithmic step. Normals computed once per scan on entry to map_buf, transformed lazily. Should reduce drift further across the board, plausibly cracking the 0.5% barrier.
3. **Highway-specific fix**: longer init range on iter 0 (currently 3.0m, but at 10m/frame on 01, this barely covers one frame of motion — try 6.0m loose with 1.5m tight kept).

Probable exp0010 = (3), since it's cheap and directly addresses the outlier; (1) and (2) for exp0011/exp0012.

## What's working (don't lose)
- Const-vel init, kd-tree NN, voxel 1.0m map storage with 0.5m source, sliding window K=20, MAX_DIST schedule, trimmed 80%, hand-rolled SVD, deterministic.
- All 9 experiments passed determinism re-run. Single-file C++17, zero external deps.

## Open questions
- Why is trimmed ICP good on 07 but bad on 10? Both are urban-ish but 07 is short with clean turns, 10 is residential with parked cars. Maybe `n_corrs` after gating is the deciding factor — if it's already constrained, don't trim further.
- Has K monotonicity finally bent? exp0007 showed 01 essentially flat from K=10→20. K=30 may regress 01 outright; informative either way.
- Walltime growing: 138 → 221 → 405 → 557 → 582 s. Still well under 30-min/seq cap, but point-to-plane will add normal-estimation cost.
