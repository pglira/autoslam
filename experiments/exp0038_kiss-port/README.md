# exp0038_kiss-port

Structural port of KISS-ICP (Vizzo et al. 2023). Reference: cloned source at https://github.com/PRBonn/kiss-icp/tree/main/cpp/kiss_icp/core. Reading notes in `journal/papers.md`.

## What's new vs all prior experiments

| Component | Before (exp0001–0037) | After (exp0038, this) |
|---|---|---|
| Local map | sliding-window deque of K=20 frames | spatial voxel hash map (`std::map`) bounded by `max_range=100m` |
| NN search | kd-tree, O(log N) per query | 27-voxel probe, O(1) per query |
| Map insertion | append, drop oldest | KISS-ICP's add (cap at N_max=20, sub-voxel dedup, prune by range) |
| ICP residual | Kabsch closed-form (centroids + SVD) | Linearized point-to-point (3D r = src−tgt), 6×6 GN |
| Outlier handling | motion-aware trim + MAX_DIST schedule | Geman-McClure kernel weight w = σ²/(σ+r²)² |
| Correspondence gate | fixed/adaptive single value | `3·σ_t` from adaptive threshold |
| Threshold update | none (fixed) | KISS-ICP adaptive τ from constant-velocity deviation |
| Voxel sizes | source 0.5m / map 1.0m (denser source) | source 1.5m / map 0.5m (denser map; KISS-ICP design) |

## Kept from prior experiments

- KITTI per-point +0.205° scan correction (exp0025, IMLS-SLAM)
- Constant-velocity warm-start
- Hand-rolled SE(3) math (no Eigen/Sophus dep)
- Single-threaded, deterministic (std::map for sorted iteration)

## KITTI config (matches KISS-ICP defaults)

- voxel_size = 1.0 m (base)
- frame_downsample at 0.5 m (α·v, stored in map)
- ICP source at 1.5 m (β·v)
- max_range = 100 m, min_range = 0
- max_points_per_voxel = 20
- initial_threshold = 2.0 m
- min_motion_th = 0.1 m
- max_num_iterations = 500
- convergence_criterion = 1e-4

## What this experiment proves or disproves

This is the FIRST of a 3-5 experiment block per the user's note about giving new methods fair airtime. Don't judge on this run alone:

- exp0038 (this): clean port at KISS-ICP defaults. May or may not beat exp0034's 0.6658%.
- exp0039: tune voxel/range params for KITTI specifically.
- exp0040: combine with scan correction tuning if needed.
- Potentially: exp0041–42 if specific sequences need attention.

## Determinism notes

- `std::map<tuple<int64,int64,int64>, vector<Vec3>>` for voxel map → sorted iteration.
- Voxel downsample uses `std::map` (first point per voxel, sorted by key).
- ICP iterates source in fixed input order.
- No threads, no RNG.

## SE(3) exponential

Proper SE(3) exp implemented from scratch:
- R = Rodrigues(phi)
- t = V(phi) · rho where V is the left Jacobian of so(3).

This replaces the small-angle approximation used in earlier point-to-plane attempts; SE(3) exp is exact for any step size.
