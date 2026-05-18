# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
*(empty — populate as soon as the first real experiments yield observations)*

## Medium priority
- [ ] Constant-velocity initialization for frame-to-frame registration — *cheap, mentioned in KISS-ICP*
- [ ] Voxel downsampling for query/source clouds — *standard speedup, almost always net-positive*
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
