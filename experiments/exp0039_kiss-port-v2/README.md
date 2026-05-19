# exp0039_kiss-port-v2

Stability fix for the exp0038 KISS-ICP port.

## What was wrong in exp0038

Every frame's ICP exited after 1 iteration (`iters=1` in run logs). σ blew up unbounded (172m on seq 00 after 200 frames). Some sequences ended up with `corrs=0` on every frame.

## Root cause hypothesis

My 6×6 Cholesky returns `false` if any diagonal pivot ≤ 0 (`if (s <= 0) return false`). Near-singular but positive-semi-definite systems trigger this. KISS-ICP uses Eigen's LDLT decomposition which has built-in pivoting and handles semi-definite matrices gracefully.

## Fix

Add Marquardt-style multiplicative regularization on the diagonal BEFORE Cholesky:
```
AtA[i,i] *= (1 + λ)   with λ = 1e-3
```

This is small enough that well-conditioned systems are essentially unaffected (changes solution by ~0.1%), but it ensures strict PD so Cholesky always succeeds.

This is essentially the LM damping I tried in exp0031, but applied to my new linearized point-to-point system (not the failed point-to-plane).

## Changes vs parent

Three lines added before `cholesky6_solve` in `align_points_to_map`:
```cpp
constexpr double LM_LAMBDA = 1e-3;
for (int i = 0; i < 6; ++i) AtA[i * 6 + i] *= (1.0 + LM_LAMBDA);
```

Nothing else changed. Same KISS-ICP defaults (voxel_size=1m, max_range=100m, N_max=20, etc.), same VoxelHashMap structure, same scan correction, same linearized p2p with GM kernel.
