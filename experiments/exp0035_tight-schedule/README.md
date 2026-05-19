# exp0035_tight-schedule

Per-iteration gate cooling schedule.

## Hypothesis

exp0034 (fixed 0.3m tight) was a net win but exposed two clear regressions:
- 01 (highway): +0.197% — fast motion → iter 1 residuals exceed 0.3m → lost matches
- 04 (short): +0.470% — only 271 frames → any frame with bad ICP destroys the metric

The mechanism is: iter 1 needs enough correspondence headroom to actually pull source onto target. Once that's done, iter 2+ can run with a tight gate to suppress outliers.

3-stage cooling schedule:
- **iter 0**: 2.0m loose (unchanged from exp0029)
- **iter 1**: 0.7m mid (was exp0032's best fixed tight value)
- **iter 2+**: 0.3m fine (was exp0034's best fixed tight value)

## Expected outcomes

- 01 recovers toward 0.79% (exp0032 / exp0033 levels)
- 04 recovers toward 0.40%
- 09, 10 retain most of their exp0034 wins (the iter 2+ tight phase is what helped them)
- Aggregate: better than exp0034's 0.666%, ideally closer to 0.6%

## Changes vs parent

- `icp()` signature: third `max_dist_fine` parameter added (was loose+tight, now loose+mid+fine).
- New `MAX_DIST_MID = 0.7` constant; `MAX_DIST_TIGHT` renamed to `MAX_DIST_FINE`.
- Iter dispatch: 0→loose, 1→mid, ≥2→fine.
