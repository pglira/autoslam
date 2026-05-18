# exp0004_frame-to-map-k5

Fork of exp0003 adding a sliding-window local map for the ICP target.

## Hypothesis

exp0003 brought the aggregate to 7.99%. The remaining error landscape is dominated by drift on long sequences (02 at 9%, 08 at 8%, 09 at 9.5%, 10 at 11%). Frame-to-frame ICP integrates per-frame error linearly — multi-frame context damps this.

Maintain a buffer of the last K=5 downsampled scans, each stored in its own V_j frame together with `M[j] = T^W_V_j`. For frame i, transform every buffered scan into V_{i-1} coords via `T^V_{i-1}_V_j = inv(M[i-1]) @ M[j]`, concatenate into a single point list, build the kd-tree on that, run ICP.

K=5 is a starting point — small enough that scene-change staleness is bounded (5 × 10 m/s × 0.1 s = 5 m of geometry), large enough that ICP has multi-frame structure to lock onto.

## Expected

- 00, 02, 08, 09, 10 — material wins (1-3% absolute each).
- 03 — might also help: country road has continuous geometry, more context is useful.
- 04 — no help (only 271 frames, but the per-sub-trajectory issue is unrelated to frame-to-map).
- Walltime ~2× exp0003 (map is ~5× more target points; kd-tree query cost is O(log N) so growth is sublinear).

## Changes vs parent (exp0003_kdtree-voxel1m)

- `<deque>` include.
- New `MapFrame` struct and `std::deque<MapFrame> map_buf`.
- Main loop now builds `target_map` from `map_buf` each iteration (transforming every stored scan into V_{i-1} coords), passes that to ICP.
- After ICP, push the new (curr, M[i]) pair and pop oldest if window exceeded.
- `WINDOW_SIZE = 5` constexpr.

Everything else (voxel 1.0 m, kd-tree, Jacobi SVD, const-vel init, MAX_DIST 3.0 m, single-thread, deterministic) unchanged.

## Risks

- Stale geometry: if vehicle moves > ~5–10 m within window, old map points are far from current observations. Mitigated by aggressive MAX_DIST gating (3 m) in ICP. Highway (01) at ~10 m/frame may push this — old K=4 frame is ~40 m behind, may not match cleanly. Worth checking if 01 regresses.
- Determinism: the local-map construction is deterministic (deque iteration order, fixed transforms). Re-run should be bit-exact.
