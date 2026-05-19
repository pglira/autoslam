# exp0013_p2pl-trim

Disambiguating exp0012's regression.

## Hypothesis

exp0012 dropped the trim block and regressed. The trim alone was worth ~0.20% on dev (exp0008 0.91 → exp0009 0.71). Possible:
- Regression was entirely from missing trim → exp0013 should win.
- Point-to-plane is genuinely worse on this pipeline → exp0013 stays worse.

The sort key for trim is now the squared **point-to-plane** residual `(n·(s_t - t))²`, not the euclidean d² used in exp0009. Motion-aware skip preserved from exp0011.

## Changes vs parent (exp0012)

- `icp_p2pl()` signature gains `trim_keep_frac`.
- Refactored from streaming AtA accumulation to two-pass: collect P2plCorr (src + Jacobian + residual), optionally sort+trim, then accumulate.
- Sort comparator: primary key `r²`, lex tiebreak on `src` coords.
- Main loop computes motion-aware `trim_frac` (1.0 if fast, 0.8 if slow).
- Restored constants `TRIM_KEEP_FRAC = 0.8`, `FAST_MOTION_M = 2.0`.

Memory: `P2plCorr` is 7 doubles + tiebreak vec3 = 80 bytes. At ~12k corrs, ~960 KB extra per ICP iter. Fine.

## Outcomes that matter

- If dev < 0.7129% (exp0011): p2pl + trim is the way, normal quality OK at k=8.
- If dev between 0.7129 and 0.8541: improvement vs exp0012 but still net loss vs exp0011. Both effects matter.
- If dev ≥ 0.8541: trim wasn't the culprit. Normal quality is. Next: exp0014 with k=20.
