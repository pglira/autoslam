# exp0034_tight-03

Probe whether 0.3m is past the tight-gate floor.

| Exp | MAX_DIST_TIGHT | Full AGG | Δ |
|---|---|---|---|
| 0011 | 1.5 m | 0.96% | — |
| 0027 | 1.0 m | 0.76% | −0.20 (+ scan correction) |
| 0032 | 0.7 m | 0.74% | −0.02 |
| 0033 | 0.5 m | 0.71% | −0.03 |
| 0034 (this) | **0.3 m** | ? | ? |

## Canary sequences

Seq 07 (tight loops) and seq 09 (mixed urban-rural bends) have iter-1 residuals near the upper end of typical (~30 cm). If 0.3m drops their valid correspondences, those two regress while others improve.

## Decision rule

- If AGG improves: trend continues, exp0035 candidates are 0.2m or a per-iter cooling schedule.
- If AGG flat or marginal regress: 0.3m is at the floor; pivot to other axes.
- If specific sequences (07, 09) regress sharply: floor confirmed near 0.5m; future tightenings should be paired with a schedule, not a single value.

## Changes vs parent

Single line: `MAX_DIST_TIGHT` 0.5 → 0.3 m.
