# exp0009_trimmed-icp-80

Trimmed ICP: drop the worst 20% of correspondences by distance, from iter 1 onwards.

## Hypothesis

The fixed MAX_DIST gate (3.0m on iter 0, 1.5m on iter 1+) is absolute — it can't adapt to per-frame data. After exp0008 left us with 9/11 sequences sub-1%, the residual error on the remaining sequences (01 highway 1.90%, 02/08 ~1.2%) is plausibly outlier correspondences that pass the 1.5m gate but are still wrong (e.g., matching to a different lane marker on highway, or to a moving vehicle's surface).

Rank-based trimming addresses this: after building corrs and sorting by distance, keep the top 80% closest. This trims based on per-frame distribution rather than absolute distance.

Trimming starts at iter 1 (let iter 0 use all corrs for recovery from any warm-start error).

## Predictions

- 01 (highway, 1.90%) drops materially — fast motion + sparse features = many marginal matches.
- 02, 08 (~1.2%) modest improvement — long sequences where small persistent biases compound.
- Already sub-1% sequences barely move (saturating).
- Aggregate: ~0.1–0.2% absolute improvement.

## Changes vs parent

- New `Corr { src, tgt, d2 }` struct (was `std::pair<Vec3, Vec3>`).
- ICP NN loop now records squared distance per correspondence.
- After NN, from iter 1+: sort by d2 (with deterministic lex-on-src tiebreak), truncate to keep best 80%.
- New constant `TRIM_KEEP_FRAC = 0.8`.
- `icp()` signature gains `trim_keep_frac` parameter.

## Determinism notes

- Sort comparator: primary key d2, secondary lex on src coordinates. Pure function of input data, no memory-address dependency.
- The minimum-corr threshold is 30 (after trim) to avoid pathological convergence on tiny correspondence sets.
