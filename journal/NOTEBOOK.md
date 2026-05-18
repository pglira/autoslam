# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0001_naive-p2p-icp** — dev 20.24%, **full 25.23%**, rot 0.047 deg/m.
- Per-seq full: `00=21.0 01=47.1 02=33.6 03=53.8 04=60.7 05=18.3 06=11.7 07=23.7 08=16.9 09=27.0 10=38.1`.

## Current direction
Drive down drift on the hardest sequences first. The aggregate is dragged by:
1. **01 (highway, 47%)** — fast motion, identity init can't bridge ~10 m/frame. Constant-velocity initialization is the obvious fix.
2. **04 (61%)** — only 271 frames; aggregate is dominated by 100/200/300 m sub-trajectories that are essentially the whole sequence. The ICP is failing here. Investigate: is it open-road with too few geometric constraints?
3. **03 (54%)** — tree-canopy / country road; sparse features, ICP can't lock on.
4. **08 (16.9%)** — long with loops, but oddly only mid-range error; the loops accidentally help.

The easy 80/20 wins to chase before anything fancier:
- **Constant-velocity initialization** — propagate previous frame's delta as the init. Should dramatically help 01 and probably 04. Cheap.
- **Faster NN (kd-tree)** — currently brute-force at 2.0 m voxels. With kd-tree, can drop to 1.0 m or 0.5 m voxels without busting wall-clock. More points = better gradient, especially on sparse-feature sequences (03).
- **Frame-to-map** with sliding window — kill drift on long sequences (00, 02, 08). Costlier change.

## Open questions
- On 04, why so bad? Identity scores ~61% and we're at 60.7%. Either ICP is collapsing to near-zero motion (then we're predicting identity-like trajectory) or correspondences are systematically wrong on the short, straight sequence. Look at the predicted poses for 04 next iteration.
- On 06 we get 11.7% — what's special there? Short loop, dense urban geometry. Confirms the algorithm works when conditions are favorable.
- Is `MAX_DIST=3.0 m` too lax? Many wrong correspondences on highway. Tighter gate after first ICP iteration?
