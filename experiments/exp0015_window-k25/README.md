# exp0015_window-k25

K window 20 → 25. Probing the budget bend.

## Hypothesis

exp0007 (K=20) ran full eval in 405 s. exp0014 (K=30) hit the 10-min dev cap on seq 00. The cost-per-K is roughly linear (map size scales as K). So K=25 should land somewhere around the geometric mean — full eval ~800 s, dev seq 00 probably ~8 min, just under the cap.

If K=25 fits: confirm whether the metric also improves (would expect small win, given diminishing returns trend).
If K=25 doesn't fit: 20 is the practical ceiling for this stack.

## Changes vs parent (exp0011)

Single line: `WINDOW_SIZE` 20 → 25.
