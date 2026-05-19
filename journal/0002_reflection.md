# Reflection 0002 — block exp0007 through exp0012

After 6 more experiments since reflection 0001. Aggregate trajectory:

```
exp0006_window-k10        2.1453%   (start of block, ref 0001)
exp0007_window-k20        1.7051%   (-0.44)
exp0008_src-voxel-05      1.0582%   (-0.65)  big
exp0009_trimmed-icp-80    0.9677%   (-0.09)  mixed per-seq
exp0010_adaptive-trim     0.9677%   ( 0.00)  null op, informative
exp0011_trim-motion-aware 0.9583%   (-0.01)  surgical 01 fix
exp0012_point-to-plane    DEV REJECT (full not run)
```

5 wins (exp0007, 0008, 0009, 0010-tied, 0011) and 1 reject (exp0012). Block aggregate change: 2.15 → 0.96, roughly halved.

## Block summary

The block followed two phases:
- **Phase 1 (exp0007–0008)**: continue scaling parameters that were already winning (window K and source voxel). Both produced large step-changes (0.44 + 0.65). Confirms that exp0006 was nowhere near saturation on these axes.
- **Phase 2 (exp0009–0012)**: refine the algorithm. exp0009 (trim) was net-positive but bumpy per-sequence; exp0010 chased an "adaptive trim" hypothesis that turned out to be a null op (the ratio gate never triggered); exp0011 surgically fixed 01's exp0009 regression via motion-aware gating; exp0012 went big with point-to-plane and regressed.

The block changed character around exp0009: per-experiment gains became sub-percent, and tradeoffs between sequences started appearing. Trimming helped some sequences and hurt others. This was the first time the "universal win" pattern broke.

## What worked

1. **K = 20** scaled the previous K = 10 win cleanly. Walltime grew but headroom stays huge.
2. **Decoupling source/target voxel sizes (exp0008)** was the biggest win of the block. Source 0.5 m gives ~4× more correspondences without inflating map storage.
3. **Motion-aware trim gating (exp0011)** demonstrated that *one-axis* hypotheses about a regression can be tested and isolated quickly. Cost: one experiment. Reward: surgical recovery of 01.

## What didn't

1. **Trim regressed 01 and 10 (exp0009)**. The flat 80% gate was wrong for sparse scenes.
2. **Adaptive-trim by correspondence ratio (exp0010) was a null op**. The ratio threshold was never approached — meaning the right per-frame signal isn't `n_corrs / n_source` but something else.
3. **Point-to-plane regressed (exp0012)**. Three plausible causes:
   - Trim was dropped in the same experiment (confound).
   - Normal quality at k = 8 on 1.0 m voxels may be too coarse.
   - The linearized 6-DoF system is more sensitive to outliers than Kabsch.
   
   Cannot disambiguate until exp0013 isolates trim from algorithm class.

## Surprises

- exp0010 being a *complete* null op (every per-seq value bit-identical to exp0009) was unexpected. It implies n_corrs/n_source is far from any threshold I'd reasonably set, and the ratio is not a useful adaptive signal here.
- exp0012's regression. The literature is unambiguous that point-to-plane outperforms point-to-point on this kind of data. The most likely culprit is normal estimation quality, since the rest of the implementation passed the sanity probe and converges in *fewer* iterations.
- Walltime grew from 405 s (exp0007) to 582 s (exp0008) and stayed there for the trim experiments. Point-to-plane (exp0012) ran in similar time — the normal-estimation cost was offset by ICP converging in fewer iterations.

## Open questions

1. **Was exp0012's regression caused by missing trim or by p2pl itself?** Answer in exp0013 (p2pl + trim).
2. **If exp0013 is still worse than exp0011, what's wrong with the normals?** Candidates: k too small (try k=20 in exp0014), radius-based instead of k-NN, normals on raw (not voxel-downsampled) scans. Will choose based on exp0013 outcome.
3. **10 (residential, 1.23%) is the persistent outlier we haven't cracked.** Not motion-related, not correspondence-ratio related. Possibly dynamic features (parked vehicles whose neighborhoods shift slightly between frames). Could be investigated via per-frame correspondence residual histograms.
4. **What's the next direction once point-to-plane is settled?** Likely candidates: adaptive MAX_DIST per KISS-ICP, larger K (30), or actually addressing 10's residential failure mode. Should consult the KISS-ICP paper before this block ends.

## Next block plan (exp0013 – exp0018)

- **exp0013**: point-to-plane + trim re-added (with rank by point-to-plane residual). Disambiguates exp0012.
- **exp0014**: depending on exp0013. If p2pl + trim wins: tune k for normal estimation. If still regression: revert to exp0011 ICP and try a different change (KISS-ICP adaptive distance).
- **exp0015**: read the KISS-ICP paper, write `papers.md` entry, design experiment around the most relevant idea.
- **exp0016 – 0018**: TBD by then.

## Paper reading

Not required this block (only mandated on 0/6 unproductive blocks; this block had 5 productive). Will trigger an opportunistic read in exp0015 since we're now firmly in KISS-ICP regime and their specific tricks (adaptive correspondence threshold, motion compensation choices for deskewed data) are likely directly relevant.
