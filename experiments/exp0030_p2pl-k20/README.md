# exp0030_p2pl-k20

Honest retry of point-to-plane with the suspected blame addressed.

## Hypothesis

Two prior p2pl attempts failed:
- exp0012: pre-correction baseline, k=8 normals → dev 0.85 (regress)
- exp0028: post-correction baseline, k=8 normals → full 0.91 (regress)

Both used `NORMAL_K = 8`. KISS-ICP, IMLS-SLAM, and CT-ICP all use 20+ neighbors. The reason: PCA on small samples is high-variance, especially for thin surfaces (poles, walls at oblique angles). With k=8 the smallest-eigenvector direction wavers; the resulting normals are noisier than the geometric noise level, and p2pl's residual (perpendicular distance to the tangent plane) becomes unreliable.

Single-variable test: `NORMAL_K 8 → 20`. Everything else identical to exp0028 (scan correction, MAX_DIST 3.0/1.0, voxel 1.0m map / 0.5m source, K=20 window, 6×6 Cholesky GN update, no trim).

## Predictions

- If normals were the bottleneck: AGG drops materially below 0.91% (where exp0028 landed). Anywhere below exp0029's 0.76% would be a NEW BEST.
- If p2pl is still worse than p2p in this pipeline structure: regression stays. Closes the avenue with stronger evidence (we tried the right k).

## Cost

Normal estimation: k-NN search visits more nodes per query, work ~ linear in k. Per-scan normal compute ~2.5×. Per-frame total cost still dominated by ICP. Expected wall full-eval 700-900s, within the 30 min/seq cap.

## Changes vs parent (exp0028)

Single line: `NORMAL_K` 8 → 20.
