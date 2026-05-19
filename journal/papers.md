# papers.md — literature consulted

Append-only table of papers read during reflection steps. Each row: bibtex-style key, title, URL, date read, the *one* key idea you took from it, and which experiment(s) it inspired.

| ref | title | url | date_read | key_idea | experiments_inspired |
|-----|-------|-----|-----------|----------|----------------------|
| Vizzo2023 | KISS-ICP: In Defense of Point-to-Point ICP | https://www.ipb.uni-bonn.de/wp-content/papercite-data/pdf/vizzo2023ral.pdf | 2026-05-19 | Adaptive τ = 3σ from CV deviations. Source coarser than map. Geman-McClure kernel. Note: actual KITTI rank 15 at 0.61%, not leader. | exp0016, exp0017, exp0018, exp0019 (all rejected — recipe doesn't transfer piecewise) |

## KITTI odometry leaderboard reality check (2026-05-19)

After exp0019's KISS-ICP attempts didn't pan out, queried the KITTI leaderboard. Top LiDAR-only methods (rank — method — trans% — approach):

- **2 — V-LOAM — 0.54% — LOAM with visual aid** (2015)
- **3 — LOAM — 0.55% — edge+planar feature extraction with point-to-line + point-to-plane residuals** (2014)
- 6 — CT-ICP2 — 0.58% — continuous-time elastic (2022)
- 7 — Traj-LO — 0.58% — continuous-time (2024)
- 11 — CT-ICP — 0.59% — continuous-time (2022)
- 15 — KISS-ICP — 0.61% — simple p2p (2023)

**Lesson:** KISS-ICP is rank 15, not the top. The SOTA LiDAR-only family is LOAM (2014) and CT-ICP (2022). They use:
- **LOAM**: classify points by local curvature into "edge" and "planar" sets; match edges to lines and planes to planes (different residual per feature class)
- **CT-ICP**: continuous-time pose model across a scan; deformable trajectory, not a single rigid frame

Future paper reads should prioritize these over more KISS-ICP follow-up.

## KISS-ICP SOURCE CODE READ (2026-05-19, exp0038 prep)

After exp0019's paper-only port failed, re-read the actual C++ implementation at https://github.com/PRBonn/kiss-icp/tree/main/cpp/kiss_icp/core. Critical differences from what the paper alone conveyed:

1. **No kd-tree at all.** The voxel hash map IS the spatial index. NN query: `PointToVoxel(q, v)` → look up the 27 surrounding voxel cells (3×3×3) → take closest point across them. O(1) per query.

2. **Map structure** (VoxelHashMap.cpp):
   - `tsl::robin_map<Voxel, vector<Vec3d>>` (use `std::map` for determinism in our port)
   - `AddPoints`: skip if voxel already has N_max=20 points OR if any existing point is within `sqrt(v²/N_max)` of new one (sub-voxel dedup)
   - `RemovePointsFarFromLocation`: erase voxels whose first point is beyond `max_distance` from current origin
   - Points stored in WORLD frame

3. **Two-stage voxelization (KissICP.cpp::Voxelize)**:
   - `frame_downsample` = VoxelDownsample(scan, voxel_size * 0.5) — DENSER, stored in map
   - `source` = VoxelDownsample(frame_downsample, voxel_size * 1.5) — COARSER, ICP input
   - Note: opposite to my exp0008 finding (finer source). With the full kiss-icp architecture, coarser source is correct.

4. **VoxelDownsample**: first-point-seen-per-voxel (not centroid). My pipeline uses centroid — minor difference.

5. **AdaptiveThreshold** (Threshold.cpp):
   - `model_sse_ = initial_threshold²` initial (NOT zero), `num_samples_ = 1` (counts as one sample!)
   - Update: `model_error = δ_trans + δ_rot` where `δ_rot = 2·r_max·sin(θ/2)`, `δ_trans = ||t||`. δ_rot uses r_max, NOT per-point range.
   - Only accumulate if `model_error > min_motion_threshold` (0.1m)
   - `ComputeThreshold()` returns plain `sqrt(model_sse/num_samples)` — NO ×3 here
   - The ×3 lives in KissICP.cpp: `max_correspondence_dist = 3.0 * sigma` passed to ICP

6. **Registration is LINEARIZED point-to-point** (Registration.cpp):
   - `residual = source - target` (3D vector!)
   - `J_r = [I3 | -[source]_×]` (3×6 Jacobian per correspondence)
   - 6×6 system: `J^T·W·J·dx = -J^T·W·r`, solved via `LDLT` (Cholesky LDLᵀ variant)
   - GM kernel weight: `w = σ² / (σ + ||r||²)²` — kernel scale is σ (not σ/3 as paper text suggested)
   - Update: `T_new = exp(dx) * T_curr` via Sophus SE(3) exponential
   - 500 max iters (essentially unbounded), converge on `||dx|| < 1e-4`

7. **Preprocessing** (Preprocessing.cpp):
   - Optional deskewing (skip for KITTI: already deskewed)
   - **Range filter**: keep points with `min_range < ||p|| < max_range`. Default max_range = 100m for KITTI.

8. **Config defaults for KITTI**:
   - voxel_size = 1.0m, max_range = 100m, min_range = 0
   - max_points_per_voxel = 20
   - initial_threshold = 2.0m, min_motion_th = 0.1m
   - max_num_iterations = 500, convergence_criterion = 1e-4

**Implementation note**: my exp0019 attempted adaptive τ + GM kernel + adaptive κ in one shot and the κ collapsed on smooth motion. Two probable causes:
- I was setting κ = (σ/3)² when it should be κ = σ² (and weight formula = σ²/(σ+r²)²)
- I didn't initialize σ to τ_0 = 2.0m as starting condition (Threshold counts initial as 1 sample with value initial_threshold)

For exp0038, port all 8 pieces above as one structural change. Expect 3-5 experiments before the implementation is fully fair.
