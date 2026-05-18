# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
- [ ] **Frame-to-map with sliding window** — exp0004. Now the biggest unaddressed error source: drift on long sequences. Maintain a rolling buffer of K (start K=5) recent downsampled scans, transformed into the current frame's coordinate system, as a unified ICP target. Should produce another step-change especially on 00, 02, 08, 09, 10.
- [ ] **Tighter MAX_DIST after first iter** — at 3.0 m gate, with kd-tree now able to find true NN cheaply, we can tighten. Schedule: 3.0 m on iter 0, 1.5 m on iter 1+. Cheap, complements frame-to-map.
- [ ] **Investigate 04 specifically** — 04 stuck at 16.8%, much higher than 05/07. Only 271 frames so sub-traj aggregation is fragile. Dump predicted poses and overlay with GT.
- [ ] **Larger window scan-pair ICP for highway** — 01 came down to 7.1% but it's the only fast-motion sequence; experimenting with `prev_delta` decay / damping might help if frame-to-map alone doesn't.

## Medium priority
- [x] Voxel downsampling — *done in exp0001 (2.0 m, centroid representative)*
- [x] Constant-velocity init — *done in exp0002, kept*
- [x] Kd-tree NN with finer voxel — *done in exp0003, huge win, kept*
- [ ] Point-to-plane vs point-to-point ICP — *normals cost compute but often pay back; needs estimated normals which adds machinery*
- [ ] Adaptive max-correspondence distance based on convergence — *KISS-ICP idea, complements the MAX_DIST schedule item*

## Low priority / speculative
- [ ] Continuous-time pose interpolation across scan — *unlikely needed since KITTI is deskewed*
- [ ] Online extrinsic refinement (Tr velo↔cam) — *research direction the user flagged as in-scope*
- [ ] Loop closure (scan context / radius search) — *defer until drift is the bottleneck even with frame-to-map*
- [ ] Surfel maps — *expensive extraction; needs justification*

## Done / Closed
- [x] **exp0001**: frame-to-frame ICP baseline (60.4 → 25.2% full)
- [x] **exp0002**: constant-velocity init (25.2 → 21.2% full; huge win on 01 and 04)
- [x] **exp0003**: kd-tree NN + 1.0 m voxels (21.2 → 8.0% full; universal improvement)
