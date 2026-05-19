# traj-viewer

Small GUI to visually inspect SLAM experiment trajectories.

- **Left**: list of experiments (from `experiments/expNNNN_*`), annotated with
  `keep/reject` status and `dev_trans_pct` / `full_trans_pct` from `results.tsv`.
- **Right**: top-down 2D plot of the predicted trajectory and KITTI ground-truth
  trajectory for the selected sequence. Toggle `dev` / `full` / `dev_recheck`
  pred sets and the sequence (00..10).

Top-down view = (x, z) projection of the camera-frame poses (KITTI convention,
z forward).

## Build & run

```bash
cd tools/traj-viewer
cargo run --release
```

Optionally pass a repo path:

```bash
cargo run --release -- /path/to/autoslam
```
