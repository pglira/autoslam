# exp0020_trim-95

Last experiment of the batch. Gentler trim fraction.

## Hypothesis

- exp0008 (no trim at all): full=1.06%, every seq OK.
- exp0009 (trim 80%): full=0.97%. Urban improved, 10 and 01 regressed.
- exp0011 (trim 80% motion-aware): full=0.96%. 01 recovered via motion gate.
- exp0020 (trim 95%, motion-aware): may capture some trim win without the 10 regression.

If exp0020 wins on full but not necessarily dev (dev is urban, where 80% was already correct), the row will still get marked `keep`/`reject` purely from dev. The promotion gate then determines if --full runs.

## Changes vs parent

Single line: TRIM_KEEP_FRAC 0.80 → 0.95.

## Open question for the next session

After 20 experiments, the loop is largely tuned within its current architecture (sliding-window deque map, point-to-point ICP, fixed-scale gate). To reach KISS-ICP-class numbers (~0.5%) probably requires their voxel-grid map (bounded by max points per voxel + max range). That's a significant restructure — appropriate for a fresh research arc after this batch.
