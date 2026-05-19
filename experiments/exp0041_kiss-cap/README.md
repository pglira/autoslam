# exp0041_kiss-cap

Fix the non-convergence explosion identified in exp0040.

## The bug

exp0040 (with fixed gates, AdaptiveThreshold disabled) had per-frame logging that revealed: on frame 45 of seq 07, ICP ran 501 iters without converging. The unstable T_icp had a translation of 13.36m. The constant-velocity prediction then doubled this on subsequent frames until inf at frame 51 and NaN by frame 52.

## Three fixes

1. **MAX_ITER 500 → 30**. KISS-ICP's 500 relies on Eigen's robust LDLT to always converge; my plain Cholesky doesn't.
2. **Non-convergence guard**: if `iters >= MAX_ITER` (didn't break out via `dx_norm < tol`), return `initial_guess` instead of `T_icp`. Skip the bad update; let constant-velocity carry the frame.
3. **Re-enable AdaptiveThreshold** (was disabled in exp0040 for isolation).

## Why this should work

On problematic frames (sparse corrs, degenerate geometry), the loop now exits at iter 30 instead of 500, and the bad result is discarded. The constant-velocity prediction is "good enough" for one frame's lapse. AdaptiveThreshold should then accumulate the higher model_error on the next frame and widen the gate to recover.

If many consecutive frames all fail, the adaptive threshold should grow enough to find correspondences again.

## Changes vs parent

- `MAX_ITER` 500 → 30
- ICP call followed by `if (r.iters >= MAX_ITER) new_pose = initial_guess; else new_pose = r.T;`
- Removed `FIXED_MAX_CORR_DIST` / `FIXED_KERNEL_SCALE`; restored `sigma = adaptive_threshold.compute()` and `max_corr_dist = 3 * sigma`.
- `adaptive_threshold.update(model_deviation)` re-enabled.
