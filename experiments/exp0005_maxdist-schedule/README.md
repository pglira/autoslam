# exp0005_maxdist-schedule

Tiny one-variable refinement on top of exp0004.

## Hypothesis

In exp0004 the ICP correspondence gate was 3.0 m for every iteration. With const-vel init + frame-to-map, after iter 0 the alignment is already within centimeters. A 3.0 m gate at that point allows matches that are far from the true correspondence, biasing the SVD.

Two-stage schedule: 3.0 m on iter 0 (preserves recovery from warm-start error), 1.5 m on iter 1+ (drops obviously-wrong matches).

Expected: small but consistent aggregate improvement (~0.2–0.5% absolute). Largest gains probably on urban with parked vehicles (10, 03) where dynamic-ish features could be spuriously matched.

## Changes vs parent (exp0004_frame-to-map-k5)

- `icp()` signature: `max_dist` split into `max_dist_loose` (iter 0) and `max_dist_tight` (iter 1+).
- `MAX_DIST` constant replaced with `MAX_DIST_LOOSE = 3.0`, `MAX_DIST_TIGHT = 1.5`.

That's it. Same K=5 sliding window, kd-tree, voxel 1.0m, Jacobi SVD, const-vel init.
