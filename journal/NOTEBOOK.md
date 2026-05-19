# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0029_loose-2m** — dev 0.6377%, **full 0.7593%**, rot 0.0030 deg/m.
- Per-seq full: `00=0.71 01=0.89 02=0.69 03=0.87 04=0.41 05=0.52 06=0.34 07=0.55 08=1.02 09=1.02 10=0.91`.

## Trajectory after 29 experiments
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
exp0025_kitti-scan-fix-v2 0.77%
exp0027_tight-1m          0.76%
exp0029_loose-2m          0.76%  <- current best
```

Mid-leaderboard territory: 0.76% sits below KISS-ICP rank 15 (0.61%), filter-reg rank 20 (0.65%), SiMpLE rank 17 (0.62%). Roughly equivalent to rank 25-30 on the official KITTI odometry leaderboard.

## Last block (exp0027 – exp0029)

| Exp | Change | Full AGG | Result |
|---|---|---|---|
| 0027 | MAX_DIST_TIGHT 1.5→1.0 (on scan-corrected base) | 0.7609 | NEW BEST (seq 01 1.04→0.89 standout) |
| 0028 | retry p2pl on scan-corrected base | 0.9113 | REGRESS — p2pl avenue closed (2nd failure) |
| 0029 | MAX_DIST_LOOSE 3.0→2.0 | 0.7593 | NEW BEST (marginal, noise-level) |

## Key methodology lesson (recurring)

For tight-gate / sensor-correction style changes, **dev set (00, 05, 07) systematically under-detects improvements** because the wins concentrate on highway (01) and long sequences (02, 08). Force `--full` diagnostic when:
- The change has strong literature backing (e.g., +0.205° scan correction in IMLS-SLAM/CT-ICP/KISS-ICP).
- The hypothesis predicts effect on sequences NOT in dev.
- Dev shows a near-tie (<0.01 difference from current best).

p2pl was the counter-example: dev predicted bad, full confirmed bad. So the heuristic works in both directions: when literature is unambiguous, dev under-estimates; when methodology is shaky, dev correctly catches the regression.

## Closed avenues (don't revisit without new evidence)

- **Point-to-plane ICP** with k=8 PCA normals. Two failures (exp0012, exp0028). Naive p2pl drop-in doesn't work in this sliding-window deque pipeline. SOTA p2pl methods (LOAM, CT-ICP) use additional machinery (scan-ring classification, continuous-time motion) we don't have.
- **KISS-ICP voxel scheme swap** (map 0.5m / source 1.5m). Busts compute budget in our K=20 deque structure (exp0017).
- **Random Tr extrinsic perturbations** (exp0023). Fitting-to-test methodology; rejected by user.
- **KISS-ICP adaptive τ + κ collapse on smooth motion** (exp0019). Recipe doesn't transfer without their voxel-grid map.

## Direction for next sessions

The remaining gap to top-leaderboard (~0.2% AGG to KISS-ICP, ~0.4% to LOAM family) likely requires structural changes, not parameter tuning:

1. **LOAM feature classification** — eigenvalue-based local PCA per point, separate edge vs planar features, point-to-line + point-to-plane residuals. ~200 LOC of new code. Highest-EV next attempt.
2. **Voxel-grid local map** (KISS-ICP-style with bounded N_max per voxel + r_max): structural change that would unlock proper KISS-ICP voxel sizing.
3. **Continuous-time motion model** (CT-ICP-style): deformable trajectory within a sweep. Bigger change.

Smaller tunings (current axis):
- 08 and 09 plateau at ~1.02% — long-loop drift. May be the ceiling for frame-to-map without loop closure.
- 10 residential at 0.91% — slight regression observed across multiple experiments; possibly dynamic-feature contamination.

## What's working (don't lose)
- Const-vel init, K=20 sliding window, kd-tree NN with k-NN, voxel 1.0m map / 0.5m source, MAX_DIST 2.0m loose / 1.0m tight, motion-aware gentle trim (95% keep), **KITTI per-point +0.205° scan correction**, deterministic single-threaded.
- 29 experiments, all builds clean. Wall full-eval ~630s. Single-file C++17, zero external deps.

## Open questions
- Why does seq 09 regress with scan correction (0.66 → 1.04, persistent across exp0025-0029)?
- 08 and 09 both ~1.02% — coupled to long-loop drift?
- Is LOAM feature classification worth the implementation effort vs. accepting current 0.76%?
