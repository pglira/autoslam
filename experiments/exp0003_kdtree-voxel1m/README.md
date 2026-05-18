# exp0003_kdtree-voxel1m

Fork of exp0002 with two coupled changes: kd-tree nearest-neighbor and a finer voxel.

## Hypothesis

exp0002's per-sequence breakdown is now dominated by **03 (country road, 47.6%)** and the mid-twenties cluster {02, 09, 04, 10}. The 03 hypothesis from the NOTEBOOK: 2.0 m voxels produce too few points on sparse-feature country scenes, so the ICP correspondence set is small and noisy.

Drop voxel size 2.0 m → 1.0 m: roughly 4× more points per scan, denser correspondences. Brute-force NN at that density would bust the wall-clock cap, so the second change is a deterministic median-split kd-tree built once per target frame.

Expected:
- 03 falls materially (target <25%).
- Small wins across the board (denser correspondences → better gradient at every frame).
- Aggregate ~21.24 → ~14-17%.
- Walltime comparable to exp0002 despite 4× points, because kd-tree NN is O(log N) per query vs O(N) for brute force.

## Changes vs parent (exp0002_const-vel-init)

- New `KdTree` class (median-split, deterministic via index-tiebreak in the sort comparator). ~80 lines.
- `icp()` builds a kd-tree on `target` once per call, replaces the inner brute-force NN loop with `tree.find_nn(sp_t, max_dist_sq)`.
- `VOXEL_SIZE` constant changed `2.0` → `1.0`.

Everything else identical: const-vel init, point-to-point Kabsch update, MAX_DIST = 3.0 m, MAX_ITER = 20, hand-rolled 3×3 Jacobi SVD, single-threaded, std::map-based voxel hashing.

## Determinism

- Kd-tree build sorts with a primary key (coord on axis) and a secondary key (original index) for stable tie-breaking — tree topology is purely a function of the input points.
- Single-threaded query, fixed traversal order (descend the better child first, backtrack to the other if its hyperplane is closer than current best).
- No RNG, no parallel reductions.

## Risks / unknowns

- 1.0 m voxels may yield ~10k points per scan in dense urban areas (00, 05, 06) — query cost grows like O(N_src × log N_tgt), per iter. Still cheaper than brute-force at 2.0 m, but watch wall-clock.
- If the kd-tree implementation has a subtle bug, the test signal would be a regression vs exp0002 (denser data should not be worse). Cross-check via probe on a single sequence before harness.
