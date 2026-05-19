# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0011_trim-motion-aware** — dev 0.7129%, **full 0.9583%**, rot 0.0036 deg/m.
- Per-seq full: `00=0.76 01=1.94 02=1.17 03=0.97 04=0.88 05=0.67 06=0.61 07=0.45 08=1.00 09=0.85 10=1.23`.

## Last attempts
- exp0012_point-to-plane (dev 0.85, REJECT): regression. Either dropping trim caused it, or normal quality (k=8) is insufficient.
- Reflection 0002 written. Block exp0007–exp0012 produced 5 wins and 1 reject.

## Current direction
Disambiguate the exp0012 regression in **exp0013**: re-add the (motion-aware) trim block to point-to-plane. Sort correspondences by squared point-to-plane residual `(n·(s_t - t))^2` and keep best 80% for slow-motion frames.

- If exp0013 wins (beats 0.9583 full / 0.7129 dev): trim was the load-bearing piece; commit to p2pl + trim going forward.
- If exp0013 still regresses: normal quality is the bottleneck → exp0014 with larger k (k=20) or normals on a denser (0.5m or raw) cloud.

## What's working (don't lose)
- Const-vel init, K=20 sliding window, voxel 1.0m map + 0.5m source, MAX_DIST schedule 3.0/1.5, kd-tree NN with k-NN extension, hand-rolled SVD + Cholesky + Rodrigues, deterministic.
- Single-file C++17, zero external deps. Walltime full-eval ~580 s.

## Open questions
- p2pl vs p2p — algorithm class question, answered by exp0013.
- Normal quality at coarse voxels — answered by exp0014 if needed.
- Why is 10 (residential, 1.23%) still high? Slow scene, trim fires, but the residual is stubborn. Possibly dynamic-feature contamination. Defer to a future targeted experiment.
- KISS-ICP paper read pending; aimed at exp0015 to inform later choices.
