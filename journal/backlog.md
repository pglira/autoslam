# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
- [ ] **MAX_DIST schedule** — exp0005. 3.0 m on iter 0, 1.5 m on iter 1+. Cheap one-variable change.
- [ ] **Point-to-plane ICP with normals on local map** — exp0006 (likely). Estimate normals via PCA on local k-NN around each map point; minimize point-to-tangent-plane error. Step up to KISS-ICP-class numbers.
- [ ] **Investigate 10 (residential, 5.57%)** — second-highest sequence after 03. Possibly dynamic-object contamination of correspondences.
- [ ] **Window size ablation** — try K=3 and K=10. Likely small effect but informative.

## Medium priority
- [x] Voxel downsampling — *done in exp0001 (2.0 m, centroid)*
- [x] Constant-velocity init — *done in exp0002, kept*
- [x] Kd-tree NN with finer voxel — *done in exp0003, huge win, kept*
- [x] Frame-to-map with sliding-window — *done in exp0004 (K=5), huge win, kept*
- [ ] Adaptive max-correspondence distance per KISS-ICP §III-C — *complements MAX_DIST schedule*
- [ ] Outlier rejection in correspondence set — *trimmed ICP or MAD-based*

## Low priority / speculative
- [ ] Continuous-time pose interpolation across scan — *unlikely needed, KITTI deskewed*
- [ ] Online extrinsic refinement (Tr velo↔cam) — *user flagged as in-scope*
- [ ] Loop closure (scan context) — *defer until drift in this regime is the bottleneck*
- [ ] Surfel maps — *expensive extraction; needs justification*

## Done / Closed
- [x] **exp0001**: frame-to-frame ICP baseline (60.4 → 25.2% full)
- [x] **exp0002**: constant-velocity init (25.2 → 21.2% full)
- [x] **exp0003**: kd-tree NN + 1.0 m voxels (21.2 → 8.0% full)
- [x] **exp0004**: sliding-window map K=5 (8.0 → 3.6% full)

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
