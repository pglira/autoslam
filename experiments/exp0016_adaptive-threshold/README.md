# exp0016_adaptive-threshold

Implements KISS-ICP's adaptive correspondence threshold (paper §III-D).

## Hypothesis

My fixed MAX_DIST schedule (3.0 m loose, 1.5 m tight) is one-size-fits-all. KISS-ICP's adaptive τ adjusts per-frame based on how much each frame deviates from constant-velocity prediction.

For frames where δ (rot + trans deviation magnitude) > δ_min = 0.1 m, accumulate the running RMS σ_t. Use τ_t = 3 σ_t for the next frame's correspondence gate.

This should:
- Be **tighter than 1.5 m** on smooth urban sequences → fewer wrong matches.
- Be **looser than 3.0 m** on rough/jerky motion → recovers from larger deviations.

## Implementation

- Replaced `MAX_DIST_LOOSE` / `MAX_DIST_TIGHT` schedule with single `tau` (starts at τ_0 = 2.0 m).
- ICP signature now takes `max_dist` (single value) instead of two.
- Per frame after ICP: compute `δ = 2·r_max·sin(θ/2) + ‖Δt‖` where ΔT = T_pred^-1 · T_icp.
- `r_max = 80.0 m` (KITTI Velodyne max range).
- Update σ_t (running RMS over frames with δ > δ_min); set τ = 3σ.
- Motion-aware trim and source/map voxels unchanged.

## Risks

- If τ collapses to a tiny value (when the scene is super smooth), correspondences vanish and ICP fails to converge. Guard: τ has no explicit floor in KISS-ICP, but with their δ_min = 0.1 m there's an implicit minimum of around 0.3 m. Will watch the per-frame logs.
- Reverting to single-stage gate (no loose iter 0) may hurt on jerky frames where the const-vel init is initially off and ICP needs a wider iter-0 search. But the adaptive τ should be wide enough on average.
