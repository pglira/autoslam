# exp0031_p2pl-lm

Third p2pl retry — this time addressing solver stability.

## Hypothesis evolution

- exp0012: p2pl regressed → blamed pre-correction geometry.
- exp0028: same algorithm on corrected geometry → still regressed. Blame shifted to normal quality.
- exp0030: larger k=20 normals → worse, not better. Normal quality is NOT the issue.

What's left? **Solver instability**. On scenes dominated by a single planar surface (e.g., open roads with mostly-ground points + occasional vertical edges), the 6×6 `J^T J` matrix is near-singular along axes the correspondences don't constrain. Pure Cholesky gives a step proportional to `1/σ_min` of the SVD, which blows up.

LM damping with multiplicative diagonal scaling `AtA[i,i] *= (1+λ)` is the textbook fix:
- For well-conditioned systems: λ=1e-3 has negligible effect, behaves like GN.
- For ill-conditioned: regularizes the small eigenvalues, bounds step size.

KISS-ICP, LOAM, and Ceres-based SLAM systems all use this. My naive GN doesn't.

## Predictions

- If solver instability was the issue: AGG drops well below exp0028's 0.91%. Anywhere below exp0029's 0.76% is NEW BEST.
- If still regressing despite damping: the issue is more fundamental (e.g., the small-angle linearization combined with my Rodrigues update doesn't compose cleanly for SE(3)).

## Changes vs parent (exp0028)

Three lines added before Cholesky:
```cpp
constexpr double LM_LAMBDA = 1e-3;
for (int i = 0; i < 6; ++i) AtA[i*6 + i] *= (1.0 + LM_LAMBDA);
```

Everything else identical: k=8 normals, scan correction, MAX_DIST 3.0/1.0, voxel 1.0/0.5m, K=20.
