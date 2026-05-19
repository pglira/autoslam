# exp0025_kitti-scan-fix-v2

Re-run of the KITTI scan correction with budget headroom.

## Why exp0024 failed

exp0024 forked from exp0022 (source voxel 0.4m). The 0.4m source was already pushing the dev cap; the per-point correction overhead tipped seq 00 over.

## Setup

Fork from exp0020 (source voxel 0.5m, trim 0.95, motion-aware). Apply `correct_kitti_scan()` to every loaded scan (both frame 0 and loop frames) right after `read_kitti_bin()` and before voxel downsampling.

Constant and geometry verbatim from KISS-ICP source code:
```
THETA = 0.205°
axis  = pt × (0,0,1)        // horizontal axis ⊥ to pt
pt'   = AngleAxis(THETA, axis/|axis|) · pt
```

## Predicted

Universal small improvement (~0.03–0.10% absolute AGG). The correction is uniform across sequences, no per-seq behavior change. Walltime should grow ~5% from the extra per-point compute (negligible vs ICP).
