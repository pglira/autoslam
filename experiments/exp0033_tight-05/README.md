# exp0033_tight-05

Continue the tight-gate sweep one more step.

| Exp | MAX_DIST_TIGHT | Full AGG | Δ vs prior |
|---|---|---|---|
| 0011 | 1.5 m | 0.96% | — |
| 0027 | 1.0 m | 0.76% | −0.20 (+ scan correction) |
| 0032 | 0.7 m | 0.74% | −0.02 |
| 0033 (this) | **0.5 m** | ? | ? |

## Expected

If trend continues monotonically: -0.005 to -0.015% AGG. If 0.5m starts dropping valid matches on jerky frames: regression. Either way, this resolves where the floor is.

## Risk

- Sharp-turn frames (especially in seq 07's tight loops and 09's road bends) may have inter-frame residuals at 30-50cm on iter 1. A 0.5m gate has only ~1.5x margin there — could start losing correspondences.
- If seq 07 or 09 specifically regresses, the floor is near 0.5m.
