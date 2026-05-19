# exp0011_trim-motion-aware

Motion-aware trim gating.

## Hypothesis

exp0010 showed correspondence ratio is not a useful trim-skip signal (always > 0.40). Different signal: per-frame motion magnitude.

- Highway (01): ~10 m/frame. `prev_delta` translation magnitude reflects this.
- Urban / residential: <1 m/frame typically.

Threshold: if `|prev_delta.translation| > 2.0 m`, skip trimming. Else trim 20% as exp0009.

Expected:
- 01 recovers toward exp0008's 1.90% (was 2.14% with trim).
- 10 may or may not recover — residential motion is slow but trim hurt it. Will see.
- Urban wins from exp0009 (07, 05) preserved.

## Changes vs parent

- `icp()` gains an early-exit on `trim_keep_frac >= 0.999` (treats as no-trim sentinel).
- Main loop computes motion magnitude from `prev_delta`, picks `trim_frac` of 1.0 or 0.8 before calling `icp()`.
- New constant `FAST_MOTION_M = 2.0`.
- Removed the dead `corrs.size() > source.size() * 0.40` guard.

## Risks

- 10's regression in exp0009 wasn't predicted to come from "fast motion" — 10 is residential, mostly slow. If 10 stays bad here, the root cause is something else (dynamic features?).
- The 2.0 m threshold is a guess. Highway is far above, urban is far below, but it's possible a few transitional frames are around it.
