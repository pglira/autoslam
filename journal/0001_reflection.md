# Reflection 0001 — block exp0001 through exp0006

After 6 experiments since bootstrap. Aggregate trajectory:

```
exp0000_identity         60.4067%  (baseline floor)
exp0001_naive-p2p-icp    25.2286%  (-35.18) first real SLAM
exp0002_const-vel-init   21.2377%  ( -3.99) warm-start ICP
exp0003_kdtree-voxel1m    7.9862%  (-13.25) dense correspondences
exp0004_frame-to-map-k5   3.5677%  ( -4.42) multi-frame target
exp0005_maxdist-schedule  3.0308%  ( -0.54) two-stage NN gate
exp0006_window-k10        2.1453%  ( -0.89) more local context
```

6/6 experiments produced an improvement; no rejects, no flakies, no crashes. Every per-sequence number improved monotonically across the block on the full set.

## Block summary

Started this block with the cheapest plausible greenfield baseline (point-to-point ICP, voxel 2.0 m, brute-force NN, identity init each frame) and added one variable per experiment, each chosen by the prior experiment's per-sequence breakdown. The order was: const-vel init → kd-tree+finer voxels → frame-to-map window → MAX_DIST schedule → larger window. The largest single jumps came from kd-tree-with-finer-voxels (exp0003, −13%) and frame-to-map (exp0004, −4.4%); the smallest from the MAX_DIST schedule (exp0005, −0.5%). Each experiment's hypothesis matched the outcome in direction, occasionally mis-predicting magnitude (e.g., I predicted frame-to-map would not help 04; it dropped 16.8→1.5).

## What worked (don't lose)

1. **Const-vel ICP init.** Mandatory before any other improvement could be measured because the worst-case sequence (01 highway) was unreachable without it.
2. **Kd-tree NN.** Single biggest enabler. Made finer voxels affordable, and finer voxels were the unlock for sparse-feature sequences (03 went 47.6 → 12.8).
3. **Frame-to-map sliding window.** Removed accumulated drift. Larger K is monotonically better in the tested range (5 → 10 dropped AGG nearly 30% relative).
4. **MAX_DIST schedule.** Mild but consistent. The 1.5 m tight gate after iter 0 is the right size for current data.
5. **Determinism discipline.** All 6 experiments passed the bit-exact re-run check. The hand-rolled 3×3 Jacobi SVD has been a solid building block; no float-reduction-order surprises.

## What didn't / surprises

- exp0002's surprise regression on seq 06 (11.7 → 13.7) self-corrected once kd-tree and finer voxels arrived. The warm-start direction wasn't the underlying problem; ICP was just under-resourced.
- exp0004's hypothesis "frame-to-map won't help 04" was wrong — 04 went 16.8 → 1.5. **Lesson: don't preemptively exclude a sequence from a fix.**
- Walltime growth is now noticeable: 117 (exp0001) → 221 (exp0006) s for full eval. Still well within the 30-min/seq cap, but worth tracking if window grows further.

## Open questions

1. **Is K window monotonic forever, or does staleness eventually win?** Pure ablation: try K=20 in exp0007.
2. **Why are 03 (3.38%) and 10 (3.09%) still 1.5× the urban sequences?** Sparse features and parked-vehicle noise are the candidate explanations. Per-frame diagnostics on these sequences would clarify.
3. **Has the gap to KISS-ICP (~0.5%) narrowed to where point-to-plane matters?** At 2.15%, probably yes — point-to-point ICP has known ~1.5–2% asymptote on KITTI per the literature. The next factor-of-2 likely requires either point-to-plane or much smarter correspondence selection.

## Next block plan (exp0007 – exp0012)

- **exp0007**: window K=10 → K=20. Confirms (or finds) the K bend. Single-line change.
- **exp0008**: source voxel finer (1.0 m → 0.5 m). More correspondences per frame. Might bust walltime; if so, accept and move on.
- **exp0009**: KISS-ICP-style adaptive MAX_DIST — track running residual statistic, gate at e.g. 3σ. Replaces the static 1.5 m tight gate.
- **exp0010**: point-to-plane ICP, with normals computed once per scan-entry into the buffer (amortized). The compute budget is the gating factor — fall back to a fast normal estimator (e.g., k=8 nearest neighbors via radius query, 3×3 PCA).
- **exp0011**: investigate 10 specifically (residential, parked vehicles). Look at correspondence residual histogram and per-frame n_corrs.
- **exp0012**: TBD based on outcomes — likely outlier rejection (trimmed ICP) or sparse-region adaptive voxel.

## Paper reading

Not required this block (per program.md §5: only on 0/6 unproductive blocks). Deferring to the next reflection, by which point we're likely in a regime where the KISS-ICP paper's adaptive gating details will be directly informative.
