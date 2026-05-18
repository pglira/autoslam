# NOTEBOOK — current direction

> Current research state. Past states preserved in git history; do not append-log here.

## Current best
- **exp0002_const-vel-init** — dev 17.04%, **full 21.24%**, rot 0.036 deg/m.
- Per-seq full: `00=17.7 01=16.6 02=30.0 03=47.6 04=26.3 05=15.6 06=13.7 07=17.9 08=17.4 09=26.4 10=23.1`.

## Last delta (exp0001 → exp0002, constant-velocity init)
- Highway (01) **47.1 → 16.6** as predicted — biggest single win.
- Short straight (04) **60.7 → 26.3** as predicted.
- Residential (10) **38.1 → 23.1** — bonus win, similar dynamics.
- Country road (03) **53.8 → 47.6** — only modest improvement; sparse features still dominate.
- **Regression on 06: 11.7 → 13.7.** Small but unexpected. Hypothesis: 06 is short urban with sharp turns where const-vel extrapolates *wrong* direction across rotation discontinuities. Identity init was fine because the basin of attraction held; warm-start introduces a wrong prior. Confirm by inspecting 06 ICP convergence iters next time we modify the engine.

## Current direction
Aggregate is now dominated by 03 (country road, 47.6%) and 09/04 (~26%). The remaining 11 sequences average ~16% — drift compounding in the long urban runs is now the next-biggest pile of error to chase.

Two complementary directions, both will go in next experiments:
1. **Smaller voxels via kd-tree NN** — current 2.0 m voxels yield ~1500 points per scan in 03's sparse country road, which means correspondence sets are tiny. Drop to 1.0 m or 0.5 m, but brute-force won't fit the budget — needs kd-tree. This is the **next experiment (exp0003)**.
2. **Frame-to-map** with sliding window — will primarily attack drift on long sequences (00, 02, 08). Probably exp0004 or later.

## Open questions
- Why does 06 regress with const-vel? Hypothesis above is testable but not urgent.
- 03 country road is at 47.6%. Need diagnostics: how many correspondences per frame on 03? If <100, the ICP is essentially fitting noise.
- 08 is essentially flat at ~17% across both experiments. What's the dominant error source there — drift, miscorrespondences, or something else?

## What's working (don't lose)
- Const-vel init: KEEP. Don't revisit identity init.
- Voxel downsample with `std::map` (deterministic sorted iteration): KEEP.
- Hand-rolled 3×3 Jacobi SVD: KEEP — no external deps, deterministic, fast enough.
- Brute-force NN: REPLACE soon (limiting voxel size).
