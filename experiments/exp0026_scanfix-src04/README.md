# exp0026_scanfix-src04

Stack the two latest wins.

## Hypothesis

- exp0022 (src 0.5→0.4m): full 0.87% → 0.87% (-0.012)
- exp0025 (scan correction on exp0020 base): full 0.88% → 0.77% (-0.11)

If additive, this experiment should land near 0.76% full. The scan correction is by far the bigger effect — combining is largely about whether the 0.4m source still fits budget once the correction overhead is added.

## Risk

exp0024 (same combination) hit the dev cap on seq 00 at frame 4400/4540. Now retrying because we know exp0025 (with same correction at 0.5m source) only ran ~530s on dev — plenty of budget room. The 0.4m source pushes back up but should fit.

If seq 00 busts again, this confirms the combination's compute is genuinely over the line; back off to 0.45m next attempt.

## Changes vs exp0025

Single line: VOXEL_SIZE_SRC 0.5 → 0.4.
