# exp0032_tight-07

Continue the MAX_DIST_TIGHT sweep.

## Tight-gate trend so far

| Exp         | MAX_DIST_TIGHT      | Full AGG                      |
| ----------- | ------------------- | ----------------------------- |
| 0011        | 1.5 m               | 0.96 %                        |
| 0027        | 1.0 m               | 0.76 % (with scan correction) |
| 0029        | 1.0 m + loose 2.0 m | 0.7593 %                      |
| 0032 (this) | **0.7 m**           | ?                             |

The 1.5→1.0 transition was worth -0.20% AGG (also bundled with scan correction). The 1.0→0.7 transition may extract another small win if outlier matches near the 1.0m boundary are still biasing the SVD update.

## Risk

Sharp-turn frames (especially on 09's mixed urban-rural transition and 07's tight loops) may have residuals between 0.7 and 1.0 m on iter 1. Those frames lose correspondences and stall ICP. If 09 or 07 regresses, we've found the floor.

## Changes vs parent

Single line: `MAX_DIST_TIGHT` 1.0 → 0.7 m.
