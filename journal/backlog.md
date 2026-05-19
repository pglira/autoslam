# backlog.md — hypothesis queue

Prioritized list of ideas to try. Reorder freely. Check off when an experiment closes the question (kept, rejected, or moved to dead_ends).

Format: `- [ ] <one-line idea> — *<short rationale>*`

## High priority
- [ ] **K=20** — exp0007. Continue the K trend (5→10 was -0.88% AGG; is it monotonic?).
- [ ] **Source voxel 0.5 m** — exp0008 likely. Target stays 1.0 m. More correspondences without changing map density.
- [ ] **KISS-ICP-style adaptive MAX_DIST** — replace fixed 1.5 m tight gate with a data-driven σ tracker.
- [ ] **Point-to-plane ICP** — biggest remaining algorithmic step. Needs careful normal-estimation budget management (compute once per scan-entry into buffer, transform on use).
- [ ] **Investigate 10 (residential, 3.09%)** — possibly dynamic-feature contamination.
- [ ] **Investigate 03 (3.38%)** — still highest "real" sequence; sparse features specific failure mode?

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
- [x] **exp0005**: MAX_DIST schedule 3.0/1.5 (3.6 → 3.0% full)
- [x] **exp0006**: window K=10 (3.0 → 2.15% full)
- [x] **exp0007**: window K=20 (2.15 → 1.71% full)
- [x] **exp0008**: source voxel 0.5m (1.71 → 1.06% full)
- [x] **exp0009**: trimmed ICP 80% (1.06 → 0.97% full, SUB-1%)
- [x] **exp0010**: adaptive trim by corr/source ratio (null op)
- [x] **exp0011**: motion-aware trim skip (0.97 → 0.96% full, surgical 01 fix)
- [x] **exp0012**: point-to-plane ICP alone (REJECT — confound)
- [x] **exp0013**: p2pl + trim (REJECT, p2pl confirmed regression)
- [x] **exp0014**: window K=30 (PARTIAL — budget bust)
- [x] **exp0015**: window K=25 (PARTIAL — budget bust, K=20 is ceiling)
- [x] **exp0016**: adaptive τ (KISS-ICP §III-D) (TIED — no win alone)
- [x] **exp0017**: KISS voxel scheme (PARTIAL — map size busts cap)
- [x] **exp0018**: Geman-McClure kernel, fixed κ (REJECT — fixed scale wrong)
- [x] **exp0019**: adaptive τ + adaptive κ (REJECT — κ collapse on smooth motion)
- [x] **exp0020**: gentler trim 95% (0.96 → 0.88% full, NEW BEST)
- [x] **exp0021**: trim 0.90 sweep midpoint (REJECT, 0.95 still best)
- [x] **exp0022**: source voxel 0.4m (0.88 → 0.87% full, NEW BEST)
- [x] **exp0023**: Tr yaw perturbation (REJECT, flawed methodology)
- [x] **exp0024**: KITTI scan correction on src=0.4m (PARTIAL, budget bust)
- [x] **exp0025**: KITTI scan correction on src=0.5m (DEV reject but FULL 0.87 → **0.77 NEW BEST** after diagnostic full eval)
- [x] **exp0026**: scan correction + source 0.4m (full 0.804% regresses vs exp0025; stack interactions matter)
- [x] **exp0027**: MAX_DIST_TIGHT 1.5→1.0m on scan-corrected base (0.77 → **0.76 NEW BEST**; seq 01 standout 1.04→0.89)
- [x] **exp0028**: retry p2pl on scan-corrected base (REGRESS to 0.91; 2nd p2pl failure, avenue closed)
- [x] **exp0029**: MAX_DIST_LOOSE 3.0→2.0m (0.7593% marginal new best; loose gate no longer binding)

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
