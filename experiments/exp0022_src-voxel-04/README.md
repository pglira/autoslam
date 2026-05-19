# exp0022_src-voxel-04

Push the source-density axis once more on top of exp0020.

## Hypothesis

Largest gains in the project came from increasing source density (exp0008: 1.0 → 0.5 m, AGG 1.71 → 1.06). Going further (0.5 → 0.4 m) might extract another notch. Walltime growth roughly linear in source point count (≈ 1.5×), so 568 s → ~850 s. Still well within the 30-min/seq cap.

## Changes vs parent (exp0020)

Single line: `VOXEL_SIZE_SRC` 0.5 → 0.4.

## Risk

Beyond a certain density, the source becomes noise-dominated (raw KITTI scan has noise on the order of cm). Going too fine could regress slightly. But 0.4 m is comfortably above sensor noise.
