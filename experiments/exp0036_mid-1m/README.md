# exp0036_mid-1m

Refine the cooling schedule: bump iter-1 gate for highway.

## Hypothesis

exp0035's 3-stage schedule (2.0 / 0.7 / 0.3) fully fixed seq 04 (0.87 → 0.53) but only partially fixed 01 (0.98 → 0.94). The hypothesis: highway's ~10 m/frame motion means iter-1 residuals are larger than 0.7 m on many frames; some still exceed the mid gate.

Set `MAX_DIST_MID = 1.0` m — exactly the value exp0032 used as its fixed tight gate (and scored 0.82 on seq 01). Iter 2+ stays at 0.3 m for the precision phase that helped 09/10.

## Predictions

- 01 recovers to ~0.79–0.82 (exp0032 level).
- 04 stays at exp0035's 0.53 (still has plenty of iter-1 headroom).
- 09, 10 minimal change.
- Aggregate: better than exp0035's 0.664%, ideally 0.63–0.65.

## Changes vs parent

Single line: `MAX_DIST_MID` 0.7 → 1.0 m.
