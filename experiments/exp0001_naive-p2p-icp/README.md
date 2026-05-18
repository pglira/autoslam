# exp0001_naive-p2p-icp

First real SLAM. Greenfield, single C++17 source file, zero external deps.

## Hypothesis

Any working ICP — even the most naive variant — should crush the 60.4% identity baseline (exp0000). Drift will dominate the error budget; expect aggregate dev trans error somewhere between **5% and 15%**.

## Changes vs parent (exp0000_identity)

- Replaced identity-pose stub with real frame-to-frame ICP.
- Reads KITTI `.bin` Velodyne scans, parses `Tr` from `calib.txt`, accumulates camera-frame trajectory.

## Implementation

- **Voxel downsample**: 2.0 m voxels with centroid representative per voxel. `std::map`-based to keep iteration deterministic. Typical scan ~120k pts → ~1.5–3k pts after downsampling.
- **NN search**: brute force, gated by max correspondence distance 3.0 m. With ~2k pts per scan, ~4M ops per ICP iteration.
- **ICP**: point-to-point Kabsch update, max 20 iters, convergence at Δt < 1e-3 m and Δrot < 1e-3 rad. Identity init each frame.
- **SVD**: 3×3 via cyclic Jacobi eigensolver on HᵀH, then `U = H V Σ⁻¹`. Hand-rolled, deterministic sweep order, no external dep.
- **Pose accumulation**: in Velodyne frame. `M[i] = M[i-1] @ ΔM`, where `M[0] = Tr`. Output `T^W_C[i] = M[i] @ Tr⁻¹` in camera frame, KITTI convention.

## Determinism

- Single-threaded.
- No RNG.
- `std::map` (not `unordered_map`) for voxel hash, so iteration order is sorted on `(ix, iy, iz)` keys.
- Centroid sums are in input file order (which is deterministic by `%06d.bin` lexicographic frame index).
- 3×3 Jacobi eigensolver uses a fixed cyclic pivot sequence `(0,1)→(0,2)→(1,2)` with hard iteration cap.

## Knowns/limits

- No constant-velocity init → ICP starts from identity each frame. On highway sequences (~10 m/frame), this means the first iteration has to bridge a 10 m offset, which exceeds the 3.0 m NN gate. **Expect 01 to be very bad.**
- No frame-to-map → drift compounds linearly with frame count.
- No loop closure → long sequences (00, 08) will drift heavily.
- Brute force NN at ~2k×2k → fine for 2.0 m voxels but won't scale to finer voxels.

## Next experiments (predictions)

- **exp0002**: constant-velocity init — should help highway/01 hugely.
- **exp0003**: kd-tree NN, finer voxels (1.0 m or 0.5 m) — better accuracy without busting the wall-clock cap.
- **exp0004**: frame-to-map with sliding window — kill drift on long sequences.
