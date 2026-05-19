# exp0017_kiss-voxel

Swap voxel scheme to match KISS-ICP: map denser (0.5 m), source sparser (1.5 m).

## Hypothesis

KISS-ICP uses `α·v = 0.5 m` for the map and `β·v = 1.5 m` for the ICP source (v = 1.0 m). This is the opposite of my exp0008 which found that finer source wins over finer map. They achieve 0.50% AGG on KITTI; I'm at 0.96%.

Two possible truths:
- **My exp0008 path is correct** but the rest of my pipeline (no robust kernel, no adaptive τ tuned to this voxel scheme) is what's leaving 0.5% on the table.
- **KISS-ICP's voxel choice is genuinely better** when paired with their other tricks (adaptive τ, robust kernel).

This experiment isolates voxel-size choice. Adaptive τ from exp0016 retained. Robust kernel still absent. If exp0017 regresses cleanly, my voxel scheme is fine and the gap is elsewhere. If exp0017 wins, KISS-ICP's recipe is preferred.

## Changes vs parent (exp0016)

Single change: VOXEL_SIZE 1.0 → 0.5, VOXEL_SIZE_SRC 0.5 → 1.5.

Effect on point counts:
- Map per frame: was ~3k pts → now ~8-10k pts. With K=20, total map ~160-200k pts.
- Source per frame: was ~12k pts → now ~1-1.5k pts.
- ICP per iter: was 12k × log(60k) ≈ 200k ops → now 1.5k × log(180k) ≈ 27k ops. Way faster.
- KdTree build (target): O(N log N) on larger N. 180k × 18 = 3.2M ops per frame. Will be the new bottleneck.

Expected wall: slight decrease (smaller source dominates).
