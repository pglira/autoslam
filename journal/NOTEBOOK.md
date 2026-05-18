# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0004_frame-to-map-k5** — dev 3.29%, **full 3.57%**, rot 0.011 deg/m.
- Per-seq full: `00=3.69 01=3.11 02=3.98 03=5.90 04=1.45 05=2.56 06=1.25 07=3.20 08=3.77 09=3.46 10=5.57`.

## Last delta (exp0003 → exp0004, K=5 sliding-window map)
- Universal improvement. Aggregate 7.99 → 3.57 (-55% relative).
- **04 cratered: 16.76 → 1.45 (−15.31).** My prediction that frame-to-map wouldn't help 04 was wrong — the local map gives short sequences enough context to anchor properly. Lesson: don't preemptively gate which sequences a fix can help.
- 03 (country road) 12.81 → 5.90 — still the highest "normal" sequence.
- 10 (residential) 5.57 — second highest, mild drift.
- Walltime 66 → 138 s, still well within cap.

## Current direction
Approaching state-of-the-art naive ICP territory (KISS-ICP-class results sit around 0.5%, so we have headroom). Remaining heads to chase:

- **03 (5.90%) + 10 (5.57%)** are the top two now. Both involve sparser geometry segments. Investigate per-frame correspondence counts and convergence patterns there.
- **MAX_DIST schedule** — at 3.0 m gate during all iters, fast/large motion frames still occasionally pull in wrong correspondences. Tighten after iter 0. Cheap, deterministic, mild win expected. Could combine in exp0005.
- **Point-to-plane ICP** — meaningful step up. Compute normals on the local map (PCA on local k-NN around each point), then minimize point-to-tangent-plane distance. Costlier but should match the 1-3% regime. Probably exp0006 or exp0007.
- **Window size sweep** — is K=5 the right choice? Smaller K = less stale geometry. Larger K = more context. Could be a quick ablation, but probably small effect.

## Plan for next experiments
1. **exp0005**: MAX_DIST schedule (3.0 m → 1.5 m after iter 0). Tiny code change, complements frame-to-map.
2. **exp0006**: point-to-plane ICP (normals on the local map). Bigger change. Triggers reflection step (will be 6 since bootstrap).
3. **exp0007+**: TBD based on results — possibly window size sweep, or adaptive correspondence distance per KISS-ICP §III-C.

## Open questions
- Why is 10 still relatively high (5.57%)? Residential with parked cars — maybe many dynamic-ish features messing up correspondences? Worth investigation.
- Window K=5 sized by what intuition? Smaller windows on highway, larger on slow urban?
- Should map_buf store the world-frame points instead of V_j-frame + M[j]? Same computation but world storage avoids the per-iter recompute. Defer — current works.

## What's working (don't lose)
- Const-vel init, kd-tree NN, voxel 1.0 m, sliding window K=5, deterministic Jacobi SVD.
- Single-file C++, zero external deps.
