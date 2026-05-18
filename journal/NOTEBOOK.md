# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0003_kdtree-voxel1m** — dev 6.85%, **full 7.99%**, rot 0.023 deg/m.
- Per-seq full: `00=7.5 01=7.1 02=9.0 03=12.8 04=16.8 05=5.8 06=6.3 07=5.7 08=8.0 09=9.5 10=10.9`.

## Last delta (exp0002 → exp0003, kd-tree + 1.0 m voxels)
- Universal win — every sequence improved.
- 03 (country road) **47.58 → 12.81** as predicted; correspondence-count starvation was the issue.
- 02 (long urban) **29.97 → 8.99** — surprise; denser correspondences also helped on long drifty runs.
- 09 (mixed) **26.40 → 9.53** — similar story.
- 06 regression from exp0002 fully recovered: **13.70 → 6.25**.
- Walltime full-eval **137.8 → 66.4 s** despite ~3× more points per scan — kd-tree is amortized faster than brute force.

## Current direction
We've moved from "naive baseline" to "competent ICP" in three experiments (60.4 → 25.2 → 21.2 → 8.0). The remaining error landscape:

| Tier | Sequences | Trans% | Dominant error source |
|---|---|---|---|
| Low | 05, 07, 06 | 5–7% | Probably near the frame-to-frame ICP ceiling |
| Medium | 00, 01, 02, 08 | 7–9% | Accumulated drift on longer runs |
| Higher | 03, 09, 10 | 9–13% | Sparse features + drift |
| Worst | 04 | 16.8% | Short sequence (271 frames); sub-traj denominator small |

The biggest remaining aggregate-mover is **drift on the long sequences**. To attack it, the next step is **frame-to-map with a sliding window** (exp0004): keep the last K frames' downsampled points, transform into the current frame's coordinate system as a unified "local map" target, ICP source = current scan against that map. Eliminates per-frame integration error.

Secondary direction: 04's 16.8% is suspicious. Only 271 frames, but ICP should still work. Worth a targeted look — possibly the sub-traj aggregation is brittle on very short paths (only 3 buckets active: 100m, 200m, 300m).

## Open questions
- For exp0004 (frame-to-map): how large a window? Too short → just smoothing across 2-3 frames; too long → stale geometry causes wrong correspondences when scene changes. KISS-ICP uses ~100 m of map. Start with K=5 frames as a baseline, tune later.
- 04 still 16.8% — is the SLAM actually broken there, or is the metric noisy on short sequences? Inspect predicted vs GT trajectory directly.
- We're approaching the regime where each experiment yields ~1–3% improvement instead of 4–13%. Reflection cadence should still trigger at the 6-experiment mark; we're at 3 experiments since the last reflection (which was bootstrap).

## What's working (don't lose)
- Const-vel init: KEEP.
- Voxel downsample 1.0 m + `std::map` deterministic iteration: KEEP.
- Kd-tree NN: KEEP. Build-once-per-target-frame inside ICP.
- 3×3 Jacobi SVD: KEEP, no dep, fast enough.
- C++17 single-file, zero external deps: KEEP for now. Will need a vendored kd-tree or PCL later if exp0004+ get more complex, but holding off as long as possible.
