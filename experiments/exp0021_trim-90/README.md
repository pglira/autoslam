# exp0021_trim-90

Single-line sweep on TRIM_KEEP_FRAC, midpoint between 0.80 (exp0011) and 0.95 (exp0020).

## Hypothesis

The trim parameter is currently the only knob with a curve:
- 0.80: 07 wins (0.45%), 10 regresses (1.23%) — too aggressive.
- 0.95: 10 recovers (0.94%), 07 regresses slightly (0.52%) — too gentle.
- 0.90 might capture more of each.
