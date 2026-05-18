# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
- [ ] **Constant-velocity init** — propagate previous Δpose as the next ICP init. Expected: huge win on seq 01 (highway, 47% → maybe 5-15%). Likely also helps 04 and 10.
- [ ] **Kd-tree NN** with finer voxel (1.0 m) — would let us actually fit more points within wall-clock; especially on sparse-feature sequences (03). Brute-force is currently the bottleneck on voxel size.
- [ ] **Tighter MAX_DIST after first iter** — at 3.0 m gate, fast motion gets wrong correspondences. Try: 3.0 m on iter 0, then shrink to e.g. 1.0 m on subsequent iters.
- [ ] **Investigate seq 04 specifically** — why 60.7% with ICP, almost as bad as identity? Dump predicted trajectory, compare visually to GT. Either ICP is collapsing or short-sequence aggregation is fragile.

## Medium priority
- [x] Voxel downsampling for query/source clouds — *done in exp0001 (2.0 m voxels)*
- [ ] Point-to-plane vs point-to-point ICP — *normals cost compute but often pay back*
- [ ] Frame-to-map with sliding-window map — *reduces drift vs frame-to-frame*
- [ ] Adaptive max-correspondence distance based on convergence — *KISS-ICP idea*

## Low priority / speculative
- [ ] Continuous-time pose interpolation across scan — *unlikely needed since KITTI is deskewed*
- [ ] Online extrinsic refinement (Tr velo↔cam) — *research direction the user flagged as in-scope*
- [ ] Loop closure (scan context / radius search) — *defer until drift is the bottleneck*
- [ ] Surfel maps — *expensive extraction; needs justification*

## Done / Closed
*(none yet)*
