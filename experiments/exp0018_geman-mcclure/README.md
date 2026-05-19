# exp0018_geman-mcclure

Adds the Geman-McClure robust kernel to the Kabsch update.

## Hypothesis

Current ICP treats all correspondences inside the MAX_DIST gate equally. A correspondence at the edge of the gate (e.g., 1.5 m residual) contributes the same weight as a tight match (0.1 m residual). The Geman-McClure kernel applies smooth downweighting:

```
w_i = κ / (κ + d_i²)²
```

with `κ = 0.25 m²` (σ ≈ 0.5 m). At d=0, w=1/κ=4. At d=0.5m (one σ), w=1.0 (normalized later). At d=1.5m, w drops to ~0.1.

The integral cost is the Geman-McClure ρ function; the per-iteration weighted Kabsch is a fixed-point iteration of IRLS for that cost.

## Predictions

- All sequences improve slightly (downweighting refines the optimum).
- Biggest gains where correspondences are noisy: 10 (residential), 03 (country road), 01 (highway). Same sequences that resisted the hard trim in exp0009.
- AGG below 0.96%. Hope for ~0.85%.

## Changes vs parent (exp0011)

- After the existing trim block: compute `w_i = κ / (κ + d²)²` per remaining corr.
- Use weighted sums for centroids, weighted outer products for H.
- Convergence check unchanged.
- New constant `GM_KAPPA = 0.25` inside the icp() function.

## Determinism notes

Weights are deterministic functions of input data. Sums in fixed corr-iteration order. Same trim + gate as exp0011.
