# exp0037_motion-tight

Per-frame motion-aware tight gate, replacing the per-iter cooling schedule.

## Why per-frame and not per-iter

exp0035/0036 tried cooling schedules that gave iter 1 more headroom (0.7m or 1.0m) before iter 2+ tight (0.3m). They only marginally helped seq 01 (0.94% vs target ~0.79%). The schedule fixed the wrong scope.

The actual issue: highway's POST-WARMUP residuals are 0.3-0.5m. So iter 2+ at 0.3m systematically drops them, regardless of iter 1.

The right scope is per-frame: highway frames need the WHOLE iter 1+ phase at a looser gate. Same machinery as motion-aware trim.

## Setup

- `MAX_DIST_LOOSE = 2.0` (iter 0, unchanged)
- `MAX_DIST_TIGHT_SLOW = 0.3` (iter 1+ on urban/slow frames)
- `MAX_DIST_TIGHT_FAST = 0.5` (iter 1+ on highway/fast frames, where `|prev_delta translation| > 2m`)

This is consistent with the motion-aware trim threshold (also 2m).

## Predicted

- 01 (highway): ~0.79 (back to exp0033's level)
- 09, 10: retain exp0034's wins (~0.73, ~0.63)
- 04 (short, slow): stays at 0.3m gate; but 04 was only hurt in exp0034 because of the tight gate's interaction with short trajectory aggregation; should mostly recover
- AGG: target ~0.62-0.63

## Changes vs parent (exp0034)

- Rename `MAX_DIST_TIGHT` → split into `_SLOW` (0.3m) and `_FAST` (0.5m).
- Main loop picks one based on `|prev_delta| > 2m`.
- Schedule from exp0035/0036 NOT used here (back to single per-frame tight value).
