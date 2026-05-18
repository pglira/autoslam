# exp0007_window-k20

Window K: 10 → 20.

## Hypothesis

exp0006 showed K=5→10 monotonically helps every sequence. Two possible patterns:
- K continues to be monotonic. Then K=20 wins, and a future experiment can find where staleness eventually bites.
- K=10 was near the bend. K=20 may regress, particularly on fast (01) or turning (07) sequences where 2 s of old geometry doesn't match current view.

Walltime will roughly double over exp0006's 221s → ~400s, still well under the 30 min/seq cap.

## Changes vs parent

Single line: `WINDOW_SIZE` 10 → 20. Everything else identical.
