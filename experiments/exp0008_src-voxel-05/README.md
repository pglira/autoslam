# exp0008_src-voxel-05

Decouple source and target voxel sizes.

## Hypothesis

The ICP source (current scan being aligned) and the target (local map) don't need the same voxel resolution. A denser source means more correspondences per iteration, which means a better-conditioned SVD/Kabsch update — same as the kd-tree+1.0m unlock for the target in exp0003. Keep the map at 1.0m (memory/build cost stays bounded), drop source to 0.5m.

## Predictions

- Universal mild win, biggest on long sequences (02, 08) where small per-frame errors compound.
- Walltime ~4× exp0007 (405s → ~1500s wall on the slowest seq, ~25 min). Under the 30 min/seq cap but the closest we've been.

## Changes vs parent

- New constant `VOXEL_SIZE_SRC = 0.5` (target stays at `VOXEL_SIZE = 1.0`).
- In the per-frame loop: downsample raw scan twice — once at 0.5m for ICP source, once at 1.0m for map_buf storage.
- Logging now reports both densities.

## Risks

- If walltime busts the cap on the slowest sequence, it gets penalized with a 100% trans error contribution. This would invalidate the experiment as a learning signal. Estimate is conservative but uncertain.
- Cache pressure: 12k source pts × 15-deep tree traversals = 180k ops per iter, all fitting in L2 ideally. Should be fine.
