# exp0012_point-to-plane

Point-to-plane ICP. The big algorithmic step after a string of micro-tweaks.

## Hypothesis

Point-to-point Kabsch minimizes sum-of-squared euclidean distances between matched pairs. It treats every misalignment equally, regardless of surface orientation. For LiDAR scans of mostly planar surfaces (roads, walls), point-to-plane is well known to converge faster and to a better local minimum because it penalizes only motion normal to the surface.

Expect a material aggregate drop. Likely 0.5–0.8% absolute reduction across most sequences; biggest on structured urban (00, 02, 05, 06, 07, 08).

## Changes vs parent (exp0011_trim-motion-aware)

- **k-NN search added to `KdTree`.** Heap-based, deterministic via lex tiebreak on (d², idx).
- **`estimate_normals(pts, k)`** function: builds a tree on the scan, finds k=8 NN per point, computes 3×3 covariance, takes the smallest-eigenvector (via existing Jacobi solver) as the normal.
- **`MapFrame` struct gains `nrm` vector** (normals alongside points, both in V_j coords).
- **Local-map build now transforms normals** with the rotation part of T^V_{i-1}_V_j (no translation, since normals are free vectors).
- **`icp_p2pl(source, target, target_nrm, ...)`** replaces point-to-point ICP:
  - Per ICP iter: for each source point's NN match in target, compute scalar residual `n · (s_t - t)`, accumulate 6×6 normal equations `J^T J` where `J = [s_t × n; n]`.
  - 6×6 Cholesky solve for `xi = (phi, rho)`.
  - Update: `T_new = exp(xi) · T_curr`, with Rodrigues for the rotation part.
- **6×6 Cholesky solver** and **Rodrigues rotation builder** added.
- **Trim block dropped** for clean one-variable comparison. Will reintroduce in exp0013 if point-to-plane wins.

## Risks / unknowns

- Normal estimation adds ~100 ms per scan. Per-seq cost +~7 min on seq 00; total well within 30-min cap but the largest budget grab yet.
- 6×6 system may be ill-conditioned for poorly-constrained motion (e.g., a planar-only correspondence set fails to constrain Z). Cholesky returns false → loop breaks. The init from const-vel should keep this rare.
- Normal sign is arbitrary (no view-direction normalization). For the residual `n · (s_t - t)` and its Jacobian, sign cancels in the squared cost, so this is OK in principle but worth keeping in mind if numerics misbehave.
