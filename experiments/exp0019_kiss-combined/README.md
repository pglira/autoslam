# exp0019_kiss-combined

Combine the two KISS-ICP pieces I tested separately: adaptive correspondence threshold τ_t = 3σ_t, and Geman-McClure kernel with adaptive κ_t = (σ_t/3)².

## Hypothesis

exp0016 (adaptive τ alone): tied with exp0011.
exp0018 (Geman-McClure with fixed κ=0.25 m² alone): slight regression.

KISS-ICP applies both together. The kernel and the gate share `σ_t` — when σ is large (jerky data), gate widens AND kernel is more permissive; when σ is small (smooth data), gate tightens AND kernel sharpens. The coupling might be what makes them work.

Trim block dropped (kernel does soft outlier rejection; KISS-ICP doesn't trim).

## Changes vs parent (exp0016)

- `icp()` now takes `kappa_sq` instead of `trim_keep_frac`.
- Removed rank-trim path inside icp().
- Centroids and H both weighted by w_i = κ_sq / (κ_sq + d²)².
- Main loop computes κ_sq = (σ_for_kappa / 3)² where σ_for_kappa is the running RMS of δ (or TAU_0/3 when not yet warmed up). κ_sq floor at 1e-4.

## Risks

- κ from σ_t may collapse on smooth motion (σ small → κ small → kernel very sharp, exclude valid mid-range matches). The floor at 1e-4 m² (σ_κ = 0.01 m) keeps this finite but tight.
- Dropping motion-aware trim could re-trigger the 01 highway regression. We'll see if the kernel compensates.
