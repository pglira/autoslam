# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0020_trim-95** — dev 0.6790%, **full 0.8847%**, rot 0.0032 deg/m.
- Per-seq full: `00=0.76 01=1.91 02=1.03 03=0.88 04=0.80 05=0.56 06=0.56 07=0.52 08=0.97 09=0.71 10=0.94`.

## Trajectory after 20 experiments
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
```

KISS-ICP territory (~0.5%) still ~1.8× away. The remaining gap likely needs structural changes (KISS-style voxel grid map instead of sliding-window deque) — covered in NEXT BLOCK PLAN.

## Last batch summary (exp0016 – exp0020)

Attempted to import KISS-ICP recipe piecemeal after reading the paper:
- exp0016 adaptive τ alone: tied (0.71 vs 0.71).
- exp0017 KISS voxel scheme (map denser, source sparser): timeout, partial.
- exp0018 Geman-McClure kernel alone (fixed κ): slight regression.
- exp0019 adaptive τ + Geman-McClure with κ=(σ_t/3)²: bad regression (κ collapsed on smooth motion).
- exp0020 gentler trim (95% keep instead of 80%): **NEW BEST** 0.88%.

Negative result confirmed: KISS-ICP's individual components don't transfer without their voxel-grid-with-N_max-per-voxel map structure. The scale coupling between τ, σ_t, and the map density is real.

The positive result (exp0020) came not from the paper but from refining the trim parameter — 80% was over-aggressive on sparse scenes like residential 10 (1.23 → 0.94). Smaller, safer changes still produce wins.

## Direction for next sessions
1. **Address remaining outliers:** 01 highway (1.91), 02 (1.03), 08 (0.97), 10 (0.94). The first three are long-distance / fast-motion drift; 10 might still benefit from another tuning step.
2. **Structural change worth considering:** voxel-grid local map with bounded N_max per voxel (KISS-ICP §III-C) replacing the sliding-window deque. Would enable adopting KISS-ICP's adaptive τ and kernel under correct scaling. Significant code change (~150 LOC).
3. **Cheap tunes left to try:** TRIM_KEEP_FRAC between 0.9 and 0.95 (sweet-spot search), MAX_DIST_TIGHT around 1.0–1.3 m, source voxel at 0.4 or 0.6 m.

## What's working (don't lose)
- Const-vel init, kd-tree NN with k-NN, voxel 1.0m map + 0.5m source, K=20 sliding window, MAX_DIST schedule 3.0/1.5, motion-aware gentle trim (95% keep), deterministic single-threaded.
- Single-file C++17, zero external deps.
- Wall full-eval ~570s (well under cap).

## Open questions
- Why does seq 07 regress when trim goes from 80% to 95%? 07 may be the only dev sequence that benefits from aggressive trim — small loop with tight geometry. The other dev seqs (00, 05) prefer gentler. Worth investigating if I want every sequence sub-0.5%.
- 01 highway plateau at ~1.9% — frame-to-frame ICP ceiling, or solvable with smarter init / longer integration?
- 10 residential — still 0.94%, persistent. Maybe dynamic features. Could try a per-frame outlier rejection based on residual std-dev.
