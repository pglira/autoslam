# exp0006_window-k10

Ablation: sliding window size K=5 → K=10.

## Hypothesis

K=5 was a guess in exp0004. This experiment tests whether more local-map context helps. Two competing effects:
- **Larger K = more anchoring geometry**, reducing drift especially on long sequences (00, 02, 08, 09).
- **Larger K = more stale geometry**, especially on fast/turning sequences where the oldest window frame is far from current.

At ~10 m/s (urban) × 10 frames × 0.1 s = 10 m of accumulated motion within the window. Geometry from 10 m back is usually still visible but may be partially occluded. We'll see which effect dominates.

## Changes vs parent (exp0005_maxdist-schedule)

Single line: `WINDOW_SIZE` constant 5 → 10. Everything else unchanged.

Walltime expectation: target_map points roughly 2×, kd-tree build cost is O(N log N) — total per-frame about 1.5× slower. Within budget.
