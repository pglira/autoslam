# exp0014_window-k30

K window 20 → 30, forking from exp0011 (the working best after the p2pl detour).

## Hypothesis

K-window scaling has produced consistent wins so far (5→10, 10→20). The trend is sub-linear though:
- exp0006 (K=5→10): AGG 3.03 → 2.15 (Δ −0.88)
- exp0007 (K=10→20): AGG 2.15 → 1.71 (Δ −0.44)

Extrapolating: K=20→30 plausibly delivers Δ ≈ −0.20%, dropping AGG from 0.96 to ~0.76. Walltime grows linearly with K (more target points to kdtree-query); expect ~900s full eval.

Staleness check: K=30 × 0.1s/frame × 10 m/s = 30 m of old geometry on highway. That's borderline visible. **Seq 01 may regress** even with motion-aware trim. Will see.

## Changes vs parent (exp0011)

Single line: `WINDOW_SIZE` 20 → 30. Everything else identical.

## Outcomes that matter

- AGG drops below 0.8%: K is genuinely the right axis to keep pushing. exp0015 could try K=40.
- AGG roughly flat: K=20 was near the bend; pivot to adaptive MAX_DIST or other.
- AGG regresses: staleness wins; bend was somewhere between 20 and 30. exp0015 could test K=25.
