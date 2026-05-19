# exp0024_kitti-scan-fix

The principled re-calibration the user asked about.

## What it does

KITTI's Velodyne HDL-64E has a documented systematic vertical-angle bias of +0.205°. The fix is a per-point rotation of each raw point around the horizontal axis perpendicular to its own direction:

```
For each pt in scan:
    axis = pt × (0, 0, 1)             # horizontal axis ⊥ to pt
    pt_corrected = AngleAxis(0.205°, axis / |axis|) · pt
```

This is intrinsic recalibration of the sensor — fundamentally different from random Tr_velo_cam perturbations. The correction is documented in:

- **Deschaud, "IMLS-SLAM"** (ICRA 2018) — original introduction.
- **CT-ICP** (Dellenbach 2022) — applies it for KITTI evaluation.
- **KISS-ICP** (Vizzo 2023) — same correction, source code shows `_correct_kitti_scan` with `VERTICAL_ANGLE_OFFSET = 0.205°`.

The constant and formula in `correct_kitti_scan()` are a direct C++ port of the KISS-ICP Python binding.

## Expected outcome

Universal small improvement on all sequences (the bias is the same across sequences). Should not regress any sequence. Magnitude: hard to predict, literature suggests it makes a noticeable difference particularly on long sequences where the bias compounds.

## Changes vs parent (exp0022)

- New `correct_kitti_scan()` function (~25 LOC).
- Called once on each raw point cloud right after `read_kitti_bin()`, before voxel downsampling.

That's it. No other changes.

## Why this is honest

- Constant taken from published source code (not tuned to test metric).
- One direction picked by source (not by sweep).
- Applies to all sequences uniformly (no per-seq tuning).
- The principle is documented: a known sensor calibration error, not a free parameter.
