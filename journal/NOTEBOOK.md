# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0022_src-voxel-04** — dev 0.6346%, **full 0.8729%**, rot 0.0032 deg/m.
- Per-seq full: `00=0.72 01=1.99 02=1.07 03=0.89 04=0.77 05=0.51 06=0.57 07=0.46 08=0.95 09=0.66 10=0.91`.

## Trajectory after 25 experiments
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
exp0022_src-voxel-04      0.87%  <- current best
```

## Last batch (exp0021 – exp0025)

| Exp | What | Result |
|---|---|---|
| 0021 | trim 0.90 (sweep midpoint) | reject (0.95 > 0.90) |
| 0022 | source voxel 0.4m | **NEW BEST 0.87%** |
| 0023 | Tr yaw +0.1° perturbation | reject; flawed methodology (user flagged) |
| 0024 | KITTI scan correction (+0.205°) on src=0.4m base | partial (budget bust) |
| 0025 | KITTI scan correction on src=0.5m base | reject (slight regression) |

## Calibration takeaways (user-flagged + investigation)

- **KISS-ICP and CT-ICP are pure LiDAR odometry**; they don't touch Tr_velo_cam. Tr is applied as a fixed coord conversion downstream.
- **Random Tr perturbation is fitting-to-test**, not science. Removed from research direction.
- **The documented intrinsic recalibration** (IMLS-SLAM/CT-ICP/KISS-ICP, vertical angle offset +0.205° on KITTI HDL-64E) was applied in exp0025 and slightly regressed. Possible: my pipeline absorbs the bias elsewhere, or the KITTI odometry release has already partially corrected it.

## Direction for next sessions
1. **LOAM-style features**: classify points by local PCA eigenvalues (edge: high linearity; planar: high planarity). Use either:
   - Feature-aware downsampling (cheap step toward LOAM)
   - Full LOAM point-to-line + point-to-plane residuals (bigger change)
2. **Tighter MAX_DIST schedule** — never explored values between 1.0 and 1.5 m.
3. **Investigate seq 01 plateau** (highway, 1.99%) — drift mechanism here is consistent across all 25 experiments. Open question.
4. **Voxel grid local map** (KISS-ICP-style with bounded N_max/voxel + r_max) — would enable proper KISS-ICP adoption. ~150 LOC architectural change.

## What's working (don't lose)
- Const-vel init, K=20 sliding window, kd-tree NN with k-NN, voxel 1.0m map / 0.4m source (exp0022 setting), MAX_DIST schedule 3.0/1.5, motion-aware gentle trim (95% keep), deterministic single-threaded.
- 25 experiments, all builds clean, all keep-status experiments determinism-verified.
- Wall full-eval 708s (well under 30-min/seq cap).

## Open questions (carry-over)
- LOAM is rank 3 on the leaderboard with 0.55% — what does that imply for my path to sub-0.7%?
- Why does the KITTI scan correction regress in my pipeline when it helps in KISS-ICP? Possibly a "stack interactions matter" effect.
- 01 highway stable at ~1.9-2.0% — what's the fundamental ceiling for frame-to-map without scan-context loop closure?
