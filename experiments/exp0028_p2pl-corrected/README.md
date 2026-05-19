# exp0028_p2pl-corrected

Retry point-to-plane ICP with the wins discovered since exp0012's regression.

## Why retry?

exp0012 ran point-to-plane on the pre-scan-correction baseline (exp0011) and regressed (dev 0.85 vs 0.71). My initial diagnosis was "p2pl doesn't transfer to this pipeline." That diagnosis was likely wrong:

- The KITTI scans had a systematic +0.205° vertical-angle bias (now corrected in exp0025).
- Per-point PCA normal estimation operates on the geometry — biased geometry → biased normals → biased p2pl residuals.
- After exp0025's correction, normals should reflect true surface orientation.

Plus exp0027 tightened MAX_DIST_TIGHT to 1.0m, which complements p2pl's local-surface assumption (matches farther from the tangent plane are mostly wrong anyway).

## Setup

Fork exp0012's main.cpp (already has all p2pl machinery: k-NN, normal estimation, 6×6 Cholesky, Rodrigues update), apply two changes:
1. Add `correct_kitti_scan()` and call after every `read_kitti_bin()`.
2. `MAX_DIST_TIGHT` 1.5 → 1.0m.

No trim block (kept absent from exp0012 for clean single-variable test).

## Predictions

- If p2pl is fundamentally stronger than p2p when geometry is clean: AGG drops below exp0027's 0.7609.
- If exp0012's regression was geometry-quality-driven: this experiment should reverse it.
- If exp0012's regression was algorithm-class-driven: this still regresses.

Diagnostic either way.
