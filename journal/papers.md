# papers.md — literature consulted

Append-only table of papers read during reflection steps. Each row: bibtex-style key, title, URL, date read, the *one* key idea you took from it, and which experiment(s) it inspired.

| ref | title | url | date_read | key_idea | experiments_inspired |
|-----|-------|-----|-----------|----------|----------------------|
| Vizzo2023 | KISS-ICP: In Defense of Point-to-Point ICP — Simple, Accurate, and Robust Registration If Done the Right Way | https://www.ipb.uni-bonn.de/wp-content/papercite-data/pdf/vizzo2023ral.pdf | 2026-05-19 | Adaptive correspondence threshold τ_t = 3σ_t computed from constant-velocity-prediction deviations δ(ΔT) = 2 r_max sin(θ/2) + ‖Δt‖. δ_min=0.1m floor. τ_0=2m init. Source coarser (β·v=1.5m) than map (α·v=0.5m). Geman-McClure kernel. KITTI AGG 0.50%. | exp0016 (adaptive τ), exp0017 (voxel swap), exp0018 (Geman-McClure) |
