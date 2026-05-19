# papers.md — literature consulted

Append-only table of papers read during reflection steps. Each row: bibtex-style key, title, URL, date read, the *one* key idea you took from it, and which experiment(s) it inspired.

| ref | title | url | date_read | key_idea | experiments_inspired |
|-----|-------|-----|-----------|----------|----------------------|
| Vizzo2023 | KISS-ICP: In Defense of Point-to-Point ICP | https://www.ipb.uni-bonn.de/wp-content/papercite-data/pdf/vizzo2023ral.pdf | 2026-05-19 | Adaptive τ = 3σ from CV deviations. Source coarser than map. Geman-McClure kernel. Note: actual KITTI rank 15 at 0.61%, not leader. | exp0016, exp0017, exp0018, exp0019 (all rejected — recipe doesn't transfer piecewise) |

## KITTI odometry leaderboard reality check (2026-05-19)

After exp0019's KISS-ICP attempts didn't pan out, queried the KITTI leaderboard. Top LiDAR-only methods (rank — method — trans% — approach):

- **2 — V-LOAM — 0.54% — LOAM with visual aid** (2015)
- **3 — LOAM — 0.55% — edge+planar feature extraction with point-to-line + point-to-plane residuals** (2014)
- 6 — CT-ICP2 — 0.58% — continuous-time elastic (2022)
- 7 — Traj-LO — 0.58% — continuous-time (2024)
- 11 — CT-ICP — 0.59% — continuous-time (2022)
- 15 — KISS-ICP — 0.61% — simple p2p (2023)

**Lesson:** KISS-ICP is rank 15, not the top. The SOTA LiDAR-only family is LOAM (2014) and CT-ICP (2022). They use:
- **LOAM**: classify points by local curvature into "edge" and "planar" sets; match edges to lines and planes to planes (different residual per feature class)
- **CT-ICP**: continuous-time pose model across a scan; deformable trajectory, not a single rigid frame

Future paper reads should prioritize these over more KISS-ICP follow-up.
