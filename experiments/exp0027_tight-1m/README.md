# exp0027_tight-1m

Tighter MAX_DIST_TIGHT on top of the scan-corrected baseline.

## Hypothesis

The scan correction (exp0025) systematically shifted points. The previous 1.5 m tight gate (exp0011, picked when scans had the +0.205° bias) lets in some matches that "looked OK given the bias" but are now wrong against corrected geometry.

Specifically, if a beam-angle-biased return was matched to its "expected place" under the bias, removing the bias should pull the actual point ~5-10 cm closer. Pre-correction's good matches stay good; pre-correction's borderline-bad matches (just inside 1.5m) might now be clearly bad after correction.

Tighten to 1.0 m. Loose gate stays at 3.0 m for warm-start recovery on jerky frames.

## Changes vs parent (exp0025)

Single line: `MAX_DIST_TIGHT` 1.5 → 1.0 m.
