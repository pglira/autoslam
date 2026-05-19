# exp0040_kiss-fixed

Debugging isolation experiment for the exp0038/0039 KISS-ICP port.

## Setup

- Replaces `sigma = adaptive_threshold.compute()` with `sigma = 0.3` (fixed kernel scale).
- Replaces `max_corr_dist = 3·sigma` with `max_corr_dist = 1.0 m` (fixed gate).
- Threshold update call retained but unused (sigma constant).
- Added per-frame logging for first 10 frames: `|pose.t|`, `|delta.t|` to trace pose drift.

## Decision tree from result

| Result | Bug location |
|---|---|
| Works (low AGG) | Bug is in AdaptiveThreshold update (probably the δ_rot formula) |
| Still partial / huge σ-equivalent symptoms | Bug is in ICP loop, SE(3) exp, or map management |
| Partial only on some seqs | Map or NN-search-specific bug |

## What to look for in logs

If the first 10 frames show `|delta.t|` growing rapidly past 1.0 m/frame, the const-vel prediction is exploding because ICP is producing huge updates. If `|delta.t|` stays at ~0.5-2 m (normal KITTI motion), σ-update bug is more likely.
