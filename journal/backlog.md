# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
- [ ] **Kd-tree NN + finer voxel (1.0 m)** — exp0003. Brute-force at 2.0 m is the bottleneck on voxel size. 03 (country road, 47.6%) is now the biggest drag and likely needs more points per scan to find correspondences in sparse geometry. Build a kd-tree once per target frame; query each source point's NN.
- [ ] **Tighter MAX_DIST after first iter** — at 3.0 m gate, even with const-vel init there's slack for wrong correspondences. Schedule: 3.0 m on iter 0, 1.5 m on iter 1+. Cheap, deterministic, probably worth 1-2% aggregate.
- [ ] **Frame-to-map with sliding window** — kill long-run drift on 00, 02, 08. Keep last N (e.g. 5) frames' downsampled points as a unified target; transform into current frame's coordinate system. More expensive but should produce a significant step-change.
- [ ] **Investigate 06 regression** — exp0002 made 06 slightly worse (11.7→13.7). Hypothesis: sharp turns + wrong-direction warm start. Could probe by limiting how much rotation can be propagated.
- [ ] **Investigate 03 correspondence count** — emit n_corrs per frame in 03; if consistently low, problem is feature density, fix is finer voxels.

## Medium priority
- [x] Voxel downsampling — *done in exp0001 (2.0 m, centroid representative)*
- [x] Constant-velocity init — *done in exp0002, huge win, kept*
- [ ] Point-to-plane vs point-to-point ICP — *normals cost compute but often pay back; needs estimated normals which adds machinery*
- [ ] Adaptive max-correspondence distance based on convergence — *KISS-ICP idea, complements the MAX_DIST schedule item above*

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
