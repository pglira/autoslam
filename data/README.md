# KITTI Odometry Data

Download from https://www.cvlibs.net/datasets/kitti/eval_odometry.php (requires a free CVLIBS account).

## Required downloads (for sequences 00-10)

1. **`data_odometry_velodyne.zip`** (~80 GB) — Velodyne point clouds, all 22 sequences
2. **`data_odometry_calib.zip`** (~1 MB) — calibration + per-frame timestamps
3. **`data_odometry_poses.zip`** (~4 MB) — ground-truth poses for sequences 00-10

**Do NOT download the image zips** (`data_odometry_color.zip`, `data_odometry_gray.zip`). autoslam is LiDAR-only by design — agent code MUST NOT attempt to read camera images. The `image_0/..image_3/` directories will not exist; any code that opens them will fail.

## Unzipping

All three zips contain a top-level `dataset/` directory that merges cleanly. Unzip all three into THIS directory (`data/`):

```bash
cd /data2/repos/autoslam/data
unzip /path/to/data_odometry_velodyne.zip
unzip /path/to/data_odometry_calib.zip
unzip /path/to/data_odometry_poses.zip
```

After unzipping, the final layout MUST look like:

```
data/
└── dataset/
    ├── sequences/
    │   ├── 00/
    │   │   ├── velodyne/         # 4541 *.bin files
    │   │   ├── calib.txt
    │   │   └── times.txt
    │   ├── 01/ ... 10/           # same structure, GT available
    │   └── 11/ ... 21/           # velodyne only, no GT (held-out test set)
    └── poses/
        ├── 00.txt ... 10.txt     # GT trajectories, KITTI camera-frame format
```

Empty `sequences/00/velodyne/` ... `sequences/10/velodyne/` and an empty `poses/` directory have been pre-created as placeholders.

## Verification

After unzipping, sanity-check by running:

```bash
ls dataset/sequences/00/velodyne | wc -l    # expect 4541
ls dataset/poses                            # expect 00.txt ... 10.txt
wc -l dataset/poses/00.txt                  # expect 4541
```

## Notes

- This directory is gitignored — the harness treats it as read-only.
- KITTI Velodyne scans are already motion-compensated (deskewed). SLAM implementations should not reimplement deskewing.
- LiDAR-only: do not download or use camera images. The harness contract does not expose image paths.
- Sequences 11-21 have no ground truth and are not used by the autoslam loop (the loop scores against the official metric on 00-10 only).
