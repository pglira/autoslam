# exp0029_loose-2m

Parallel tightening to exp0027 but on the iter 0 gate.

## Hypothesis

exp0027 demonstrated that the post-scan-correction baseline had a tighter residual distribution: 1.5 → 1.0m on iter 1+ won (AGG 0.7748 → 0.7609). The same logic plausibly applies to iter 0:

- Pre-correction, the bias added a systematic residual; the 3.0m loose gate accommodated this.
- Post-correction, residuals are tighter on every iter. The iter 0 envelope is set by const-vel warm-start error, which is typically < 1m for KITTI (vehicles don't jerk between frames).

Try 2.0m. If it wins, the curve points further. If it loses, 3.0m was a real floor.

## Risk

Highway sequences (01) have ~10m/frame motion. Const-vel may occasionally be off by ~1m (acceleration). 2.0m is still 2× typical const-vel residual, should be enough headroom.

## Changes vs parent (exp0027)

Single line: MAX_DIST_LOOSE 3.0 → 2.0m.
