# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0006_window-k10** — dev 1.91%, **full 2.15%**, rot 0.0063 deg/m.
- Per-seq full: `00=2.06 01=1.93 02=2.60 03=3.38 04=0.90 05=1.64 06=0.83 07=1.89 08=2.19 09=2.00 10=3.09`.

## Direction after reflection (post-exp0006)
Just wrote `journal/0001_reflection.md`. Six straight wins. Now in the regime where each step is ~0.5–1% rather than 4–13%. Two main fronts:

1. **Continue scaling K** — exp0007 = K=20. If still monotonic, exp0008 may consider K=30 but compute budget tightens.
2. **Densify or improve correspondences** — finer source voxel (0.5 m), point-to-plane, adaptive MAX_DIST. These get us under 1.5% probably.

## Remaining error landscape
- 03 (3.38%) — country road, sparse features. Despite the big drop from 12.8%, still highest "real" sequence.
- 10 (3.09%) — residential, parked vehicles. Possibly dynamic-feature contamination.
- 02 (2.60%), 08 (2.19%) — long urban with loops; drift residual after frame-to-map.
- 06 (0.83%) and 04 (0.90%) — sub-1%, essentially saturated for current method class.

## Planning for the immediate next block
- **exp0007**: K=20.
- **exp0008**: source voxel 0.5 m (target stays 1.0 m).
- **exp0009**: TBD per outcomes.

## What's working (don't lose)
- Const-vel init, kd-tree NN, voxel 1.0 m, sliding window, MAX_DIST schedule, deterministic Jacobi SVD.
- Single-file C++, zero external deps. Walltime budget healthy at 221s full / cap 30 min/seq.

## Open questions
- Will K=20 still help, or does staleness win?
- If source voxel finer hurts walltime, can we get same gain via finer target voxel + same source?
- Point-to-plane normal estimation budget — defer until adaptive-distance ablation done.
