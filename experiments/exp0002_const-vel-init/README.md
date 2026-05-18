# exp0002_const-vel-init

Fork of exp0001 with one change: constant-velocity ICP initialization.

## Hypothesis

The exp0001 full-eval breakdown showed seq 01 (highway, 47.1%) and seq 04 (short, 60.7%) dragging the aggregate. Both have one thing in common: identity init places the ICP starting point far from the true motion. On a highway scene at ~10 m/frame, the inter-frame translation exceeds the 3.0 m NN gating distance, so frame 0's ICP can't find good correspondences and the trajectory diverges immediately.

Constant-velocity init carries forward the previous frame's estimated Δpose as the next frame's initial transform. After the first transition (still identity-initialized), every subsequent frame starts ICP from a much better location.

Expected: 01 falls from 47% to single digits. 04 should improve materially. Urban sequences (00, 05, 06, 07, 08) barely change because identity init was already inside the basin of attraction (those scenes have ≤ ~1 m/frame motion in tight geometry).

## Changes vs parent (exp0001_naive-p2p-icp)

- `icp()` now takes an `init: Mat4` argument and starts with `T = init` instead of `T = Mat4::identity()`.
- Main loop tracks `prev_delta`, initialized to identity, updated each iteration to the most recent ICP result. Passed to the next ICP call as `init`.
- For i=1 (the very first frame transition) `prev_delta` is still identity, so behavior on that single frame is unchanged from exp0001.

Everything else — voxel size (2.0 m), NN gate (3.0 m), max iters (20), determinism (single-threaded, std::map, fixed Jacobi sweep) — is identical.

## What this experiment doesn't test

- It doesn't tighten NN gate after first iter (separate backlog item).
- It doesn't change voxel size or NN strategy (kd-tree is a separate item).
- It doesn't add a map or loop closure.

This is a clean one-variable ablation. If the predicted improvement on 01 doesn't materialize, the diagnosis must look elsewhere (NN gating too lax, wrong correspondences confusing ICP, etc.).
