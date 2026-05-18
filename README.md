# autoslam

An autoresearch-style autonomous loop that designs, implements, and evaluates LiDAR SLAM methods on the KITTI odometry benchmark (sequences 00–10). Modeled after [karpathy/autoresearch](https://github.com/karpathy/autoresearch), but for LiDAR SLAM rather than ML training.

The agent (Claude Code) iteratively writes complete SLAM implementations from scratch in any compiled language of its choosing, scores them against the official KITTI metric, and accumulates a leaderboard over weeks or months — no human attendant required between sessions.

> **Three docs, three audiences.** This `README.md` is for **you** (human operator). [`program.md`](program.md) is the **agent's** prompt — read it once to understand what the agent is told. [`eval/README.md`](eval/README.md) documents the **harness internals**.

---

## What's in the repo

```
.
├── README.md              ← you are here (how to operate)
├── program.md             ← agent's prompt (read by every agent session)
├── INBOX.md               ← async message channel: you → agent
├── results.tsv            ← leaderboard, one row per experiment
├── eval/                  ← frozen harness (run_experiment.sh, vendored KITTI devkit)
├── experiments/
│   ├── exp0000_identity/  ← contract reference (NOT a SLAM — identity poses)
│   └── expNNNN_<slug>/    ← each real experiment, immutable once scored
├── journal/
│   ├── NOTEBOOK.md        ← agent's current research direction
│   ├── papers.md          ← papers consulted
│   ├── dead_ends.md       ← rejected ideas + rationale
│   ├── backlog.md         ← prioritized hypotheses
│   └── NNNN_reflection.md ← agent's notes after every 6 experiments
└── data/dataset/          ← KITTI raw data (gitignored)
```

---

## One-time setup

Already done if this repo was bootstrapped via the existing scripts; reproduced here for re-creating the environment elsewhere.

1. **Download KITTI odometry data** — see [`data/README.md`](data/README.md). You need three zips (~80 GB total): velodyne, calib, poses. LiDAR-only — do NOT download the image zips.
2. **Verify layout**:
   ```bash
   ls data/dataset/sequences/00/velodyne | wc -l    # expect 4541
   ls data/dataset/sequences/00/{calib.txt,times.txt}  # both must exist
   ls data/dataset/poses/00.txt                      # GT
   ```
   If `calib.txt` and `times.txt` are missing from `sequences/NN/`, see the symlink fix in [`data/README.md`](data/README.md).
3. **Smoke test the harness**:
   ```bash
   bash eval/run_experiment.sh experiments/exp0000_identity
   ```
   Expected: a row in `results.tsv` with `dev_trans_pct` around 60% and `status=keep` (it's the first row, so it sets the baseline).

---

## Running the agent

### Phase 1 — manual sessions (recommended for the first ~10–20 experiments)

Open a fresh Claude Code session in this directory and prompt:

> You are the autoslam agent. Read `program.md` and begin one iteration of the loop.

The agent will:
1. Run §2 setup of `program.md` (read INBOX.md, results.tsv, all journal files, exp0000 reference).
2. Pick a parent (initially: exp0000_identity, the only option).
3. Create `experiments/exp0001_<slug>/` with `build.sh`, `config_template.yaml`, `meta.yaml`, `README.md`, and source.
4. Invoke `bash eval/run_experiment.sh experiments/exp0001_<slug>` — this is automatic.
5. (If dev result beats current best by ≥0.05% trans) run `--full` to promote.
6. Commit, push, send push notification, exit.

You watch what happens, catch contract violations, and steer via INBOX.md (see below). Each session does one experiment.

### Phase 2 — scheduled / autonomous (after the contract is stable)

Use the `/schedule` skill to set up a routine that fires every ~90 minutes during a 16h/day window. Each firing = fresh Claude session that runs one experiment + (if due) one reflection + (if due) one promotion, then exits cleanly. State persists on disk so each session picks up where the last one left off.

```
/schedule
```

Then follow the prompts to create a routine that runs:

> You are the autoslam agent. Read `program.md` and run one iteration of the loop.

at the desired cron cadence.

---

## Watching the agent

You have four ways to see what the agent is doing, in increasing latency:

### 1. Push notifications (instant, on your phone)

The agent calls `PushNotification` after every experiment, every promotion, and every reflection. Connect [Remote Control](https://docs.anthropic.com) on your phone and the pushes arrive in seconds. Format examples:

```
exp0023_voxel-gicp: dev=0.78% (best=0.74%) → reject
exp0024_normal-icp: dev=0.69% NEW BEST → promoting
exp0024_normal-icp: full=0.66%, NEW LEADERBOARD BEST
exp0025_surfel-map: build_fail (CMake: Eigen3 not found)
```

### 2. GitHub commits (durable, browsable from any device)

The agent commits + pushes after every experiment. Watch [the repo](https://github.com/pglira/autoslam) on GitHub mobile to get a notification on every commit. Quick views from your phone:

- **Commits view** — chronological experiment log
- **`results.tsv` (latest)** — leaderboard, sortable in raw form
- **`journal/NOTEBOOK.md` (latest)** — agent's current direction
- **Any `experiments/expNNNN_*/README.md`** — agent's rationale for that run

### 3. `results.tsv` (when at a terminal)

```bash
column -t -s $'\t' results.tsv | less -S    # readable view
sort -t $'\t' -k6,6 -g results.tsv | head   # sort by dev_trans_pct
```

Columns: `exp parent timestamp language status dev_trans_pct dev_rot_deg_per_m dev_seq_00 dev_seq_05 dev_seq_07 full_trans_pct full_rot_deg_per_m full_per_seq dev_wall_s full_wall_s peak_rss_mb description`.

Status values: `keep` (new dev best), `reject` (worse than best), `partial` (some seq failed), `build_fail`, `crash`, `flaky` (failed determinism re-run).

### 4. `journal/` files (digestible recap)

`journal/NOTEBOOK.md` — the agent's current top-of-mind. `journal/NNNN_reflection.md` (highest N) — the most recent block recap. Read these to understand *why* the agent is doing what it's doing.

---

## Talking to the agent — `INBOX.md`

You steer the agent asynchronously by appending messages to `INBOX.md`. The agent reads it at the very start of every session, before designing the next experiment, and appends an ack inline.

### Posting a message

From anywhere — GitHub's mobile web editor, a `git push` from your laptop, the `gh` CLI, or a terminal in this repo:

```markdown
## 2026-05-19 14:22 — philipp
Stop chasing point-to-plane variants. Try scan-context loop closure next.
```

Commit + push. Within ≤90 min (or the next manual session), the agent picks it up. You'll see its ack appear under your message:

```markdown
> ack 2026-05-19 14:38 — pivoting; backlog reordered, loop closure next.
```

### Useful message patterns

| You want to … | Message |
|---|---|
| Suggest a direction | "Try X next" / "Have you considered Y?" |
| Veto a direction | "Stop pursuing X. Move on." |
| Ask for a recap | "Summarize the last 10 experiments in NOTEBOOK.md." |
| Pause iteration | "STOP — don't start a new experiment." |
| Resume | "OK, resume the loop." |
| Force a paper read | "Read paper Z (url) before next experiment." |
| Investigate a specific exp | "Investigate why exp0023 was so much worse than exp0022." |

### Hard rules the agent follows

- Never deletes or rewrites your messages.
- Never edits its own past acks.
- "STOP" or similar → acks, does not start a new experiment, exits cleanly. The next scheduled firing will see the unrescinded STOP and skip.

---

## Stopping the agent

**During a manual session**: Ctrl-C in the terminal.

**While a `/schedule` routine is active**: stop with `/schedule` skill (list, delete the routine). The next firing won't happen. An in-flight session finishes its current experiment cleanly (commits and exits).

**Via INBOX.md** (slower but works from anywhere):

```markdown
## 2026-05-19 14:22 — philipp
STOP. Don't start a new experiment.
```

Push to the remote. Next firing reads the STOP, acks, exits without starting work.

---

## Running an experiment by hand

Useful for debugging or re-scoring after modifying GT data layout. The agent is NOT supposed to do this — experiments are immutable once scored — but as the human operator you can:

```bash
# Dev set {00, 05, 07}, ≤10 min per sequence
bash eval/run_experiment.sh experiments/expNNNN_<slug>

# Full set {00..10}, ≤30 min per sequence
bash eval/run_experiment.sh experiments/expNNNN_<slug> --full
```

This will **upsert** (replace) the existing `results.tsv` row. Useful when you want to verify a result or after a harness fix.

---

## Inspecting a failure

When an experiment fails, look at:

1. **`results.tsv` row** — `status` column tells you the failure mode (`build_fail`, `crash`, `flaky`, `partial`).
2. **`run_logs/expNNNN_<slug>_build.log`** — build output (if `build_fail`).
3. **`run_logs/expNNNN_<slug>/SEQ.log`** — per-sequence stdout/stderr.
4. **`experiments/expNNNN_<slug>/README.md`** — the agent's stated hypothesis. Did the result match?
5. **`experiments/expNNNN_<slug>/preds_dev/`** — the trajectories that were scored (gitignored, present locally).

---

## Cost & rate-limit notes

- The expensive work (build + run SLAM on dev set) is CPU, not API tokens. A typical experiment uses ~5–10 minutes of Claude time for planning/coding/parsing, plus 5–30 minutes of CPU for the actual SLAM.
- At ~10 experiments/day, expect ~60–100 min of Claude usage daily — well within the 5h cap.
- If a `/schedule` firing hits a rate-limit, the agent logs to `journal/disk_log.md` and exits; the next firing tries again.

---

## What "good" looks like

| Milestone | Approx dev_trans_pct |
|---|---|
| Identity poses (current floor) | ~60% |
| Naive frame-to-frame ICP | ~5–15% |
| Voxel-downsampled point-to-plane ICP | ~1–3% |
| Frame-to-map with sliding window | ~0.7–1.5% |
| KISS-ICP-class implementation | ~0.5% |
| State-of-the-art on KITTI leaderboard | ~0.4% |

The leaderboard at https://www.cvlibs.net/datasets/kitti/eval_odometry.php is the reference for what's possible.

---

## License

TBD.
