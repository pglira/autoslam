# exp0023_tr-yaw-pos01

Calibration-axis probe: apply a small rotation to Tr.

## Hypothesis

Across 22 experiments I've improved the SLAM internals but never touched the provided Tr_velo_cam extrinsic. KITTI's calibration has documented uncertainties of ~0.1–0.5° per rotation axis. The most leverage for translation-error metric is on the yaw axis (velo Z = vehicle up): a 0.1° yaw error in Tr causes the camera-frame trajectory to swirl around the vertical, accumulating translation error over long distances.

Test: replace `Tr` with `Tr @ Rz(+0.1°)`. Run dev set. If wins, sign and magnitude are worth refining (exp0024). If loses cleanly, try -0.1° or other axes. If null (~unchanged), yaw is not the calibration bottleneck for this dataset.

## Changes vs parent (exp0022)

Single block in main(): construct `Rz(0.1°)` and set `Tr = Tr_orig @ Rz`. Everything downstream (downsampling, ICP, output) uses the perturbed `Tr` and its inverse. No other change.

## Honesty discipline

To avoid fitting to test:
- One direction picked up-front (yaw, sign + 0.1°).
- One experiment, one outcome. No iterative tuning of the angle until the dev metric improves.
- If exp0024 follows up, it's on the *hypothesis-driven* basis of which direction the gradient suggests (sign flip, larger magnitude, or different axis), not metric-driven micro-tuning.
