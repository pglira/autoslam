# exp0042_voxmap-kabsch

Hybrid: KISS-ICP's spatial-range voxel-grid map + exp0033's proven Kabsch ICP.

## Why this hybrid

exp0038-0041 (four attempts at full KISS-ICP port) all failed in the linearized point-to-point GN solve. Pose explosion to NaN was tracked to specific frames where ICP didn't converge and produced huge updates.

The voxel-grid MAP structure is orthogonal to the ICP solver. This experiment isolates the structural change: same ICP that delivered exp0033 (full=0.71%) but with map storage as KISS-ICP-style voxel grid (bounded by spatial range, not frame count).

## Implementation

- `std::map<(int64,int64,int64), vector<Vec3>>` keyed by `floor(p/v)` in WORLD frame.
- `add_to_map(pts_world)`: floor + push back, capped at N_max=20 per voxel.
- `prune_far(origin)`: erase voxels whose first point is beyond max_range=100m.
- Target for ICP: extract all map points (world frame), transform to V_{i-1} via `inv(M[i-1])` once. Build kd-tree as before. Run Kabsch ICP.
- Update map: transform curr_map (V_i frame) by `M[i]` to world, add + prune.

## Predictions

- If voxel-grid bounds map similarly to K=20 deque: AGG ≈ exp0033's 0.71%.
- If voxel grid provides better coverage on long sequences (09, 10): AGG drops.
- If voxel grid duplicates redundant near-points despite N_max cap: AGG could regress.

## Changes vs parent (exp0033)

- Removed `MapFrame` struct and `std::deque<MapFrame> map_buf`.
- Added `local_map` (std::map of vector<Vec3>), `vox_key`, `add_to_map`, `prune_far` helpers.
- Modified target_map building (extract all map pts, transform once by inv(M)).
- Removed WINDOW_SIZE; added MAX_RANGE = 100m, MAX_PTS_PER_VOX = 20.
- All other code (ICP, trim, scan correction) unchanged.
