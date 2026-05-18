#!/usr/bin/env python3
"""
autoslam harness — builds an experiment, runs it on the dev or full set,
validates output, computes the KITTI metric, and appends/updates a row in
results.tsv. Called via eval/run_experiment.sh.

Invariants this script enforces:
  - Per-sequence wall-clock cap (10 min dev / 30 min full)
  - Per-process RAM cap (8 GB virtual memory via ulimit -v)
  - Up to floor(N_cores / threads) sequences in parallel
  - Output line count must match input timestamp line count
  - Every output line must be 12 finite floats
  - Determinism re-run on dev keep-candidates: second run must be bit-exact
  - Failed sequences contribute 100% trans error to the aggregate
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
EVAL = ROOT / "eval"
DATA_SEQS = ROOT / "data" / "dataset" / "sequences"
DATA_POSES = ROOT / "data" / "dataset" / "poses"
RESULTS_TSV = ROOT / "results.tsv"
RUN_LOGS = ROOT / "run_logs"
TMP_CONFIGS = ROOT / "tmp_configs"
DEVKIT = EVAL / "devkit" / "evaluate_odometry"

DEV_SEQS = ["00", "05", "07"]
FULL_SEQS = [f"{i:02d}" for i in range(11)]

# Per-sequence caps (seconds).
CAP_DEV_S = 10 * 60
CAP_FULL_S = 30 * 60

# Per-process RAM cap (KB for ulimit -v).
RAM_CAP_KB = 8 * 1024 * 1024  # 8 GB

# Penalty assigned to failed sequences in aggregation.
PENALTY_TRANS_PCT = 100.0
PENALTY_ROT_DPM = 1.0  # 1 deg/m is absurdly high; flags rotation also broken

# Promotion threshold (absolute trans% improvement on dev required to trigger full eval).
PROMOTION_DELTA = 0.05

RESULTS_COLUMNS = [
    "exp",
    "parent",
    "timestamp",
    "language",
    "status",
    "dev_trans_pct",
    "dev_rot_deg_per_m",
    "dev_seq_00",
    "dev_seq_05",
    "dev_seq_07",
    "full_trans_pct",
    "full_rot_deg_per_m",
    "full_per_seq",
    "dev_wall_s",
    "full_wall_s",
    "peak_rss_mb",
    "description",
]


# -------------------- TSV helpers --------------------

def ensure_results_header() -> None:
    if not RESULTS_TSV.exists():
        RESULTS_TSV.write_text("\t".join(RESULTS_COLUMNS) + "\n")


def read_results_rows() -> list[dict[str, str]]:
    if not RESULTS_TSV.exists():
        return []
    lines = RESULTS_TSV.read_text().splitlines()
    if not lines:
        return []
    header = lines[0].split("\t")
    rows = []
    for line in lines[1:]:
        if not line.strip():
            continue
        parts = line.split("\t")
        # Tolerate short rows (rare, but defensive).
        parts += [""] * (len(header) - len(parts))
        rows.append(dict(zip(header, parts)))
    return rows


def write_results_rows(rows: list[dict[str, str]]) -> None:
    out = "\t".join(RESULTS_COLUMNS) + "\n"
    for r in rows:
        out += "\t".join(r.get(c, "") for c in RESULTS_COLUMNS) + "\n"
    RESULTS_TSV.write_text(out)


def append_results_row(row: dict[str, str]) -> None:
    ensure_results_header()
    line = "\t".join(row.get(c, "") for c in RESULTS_COLUMNS) + "\n"
    with RESULTS_TSV.open("a") as f:
        f.write(line)


def upsert_results_row(row: dict[str, str]) -> None:
    """Replace row with matching `exp`, else append."""
    rows = read_results_rows()
    for i, r in enumerate(rows):
        if r.get("exp") == row.get("exp"):
            merged = {**r, **{k: v for k, v in row.items() if v != ""}}
            rows[i] = merged
            write_results_rows(rows)
            return
    append_results_row(row)


# -------------------- meta.yaml parsing --------------------

def parse_simple_yaml(path: Path) -> dict[str, Any]:
    """Minimal YAML parser for the flat schema we use in meta.yaml.
    Supports: key: value, key: [a, b, c], key: null, key: true/false, key: number.
    Comments (#...) ignored. Multi-line lists not supported (kept flat on purpose)."""
    text = path.read_text()
    out: dict[str, Any] = {}
    for line in text.splitlines():
        line = line.split("#", 1)[0].rstrip()
        if not line or ":" not in line:
            continue
        if line.startswith(" ") or line.startswith("\t"):
            # Multi-line list item under a previous list key — append.
            stripped = line.strip()
            if stripped.startswith("- "):
                v = stripped[2:].strip().strip('"\'')
                last_key = next(reversed(out)) if out else None
                if last_key and isinstance(out[last_key], list):
                    out[last_key].append(v)
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()
        if value == "" or value == "[]":
            out[key] = []
        elif value == "null" or value == "~":
            out[key] = None
        elif value.lower() == "true":
            out[key] = True
        elif value.lower() == "false":
            out[key] = False
        elif value.startswith("[") and value.endswith("]"):
            inner = value[1:-1].strip()
            out[key] = [s.strip().strip('"\'') for s in inner.split(",")] if inner else []
        else:
            try:
                if "." in value:
                    out[key] = float(value)
                else:
                    out[key] = int(value)
            except ValueError:
                out[key] = value.strip('"\'')
    return out


# -------------------- subprocess helpers --------------------

def run_capture(cmd: list[str], cwd: Path | None = None, log_path: Path | None = None,
                timeout: float | None = None) -> tuple[int, str]:
    """Run command, capture combined output, optionally save to log. Returns (returncode, output)."""
    try:
        proc = subprocess.run(
            cmd, cwd=str(cwd) if cwd else None,
            capture_output=True, text=True, timeout=timeout,
        )
        out = (proc.stdout or "") + (proc.stderr or "")
        rc = proc.returncode
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") + (e.stderr or "") + "\n[TIMEOUT]\n"
        rc = 124
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(out)
    return rc, out


def n_cores() -> int:
    return os.cpu_count() or 1


# -------------------- experiment build --------------------

def build_experiment(exp_dir: Path) -> tuple[bool, Path]:
    """Run build.sh in exp_dir, log to run_logs/EXP_build.log. Returns (success, log_path)."""
    exp_name = exp_dir.name
    log = RUN_LOGS / f"{exp_name}_build.log"
    rc, _ = run_capture(["bash", "build.sh"], cwd=exp_dir, log_path=log, timeout=600)
    return rc == 0, log


def ensure_devkit() -> None:
    rc, _ = run_capture(["bash", str(EVAL / "devkit" / "build.sh")])
    if rc != 0:
        sys.stderr.write("FATAL: devkit build failed\n")
        sys.exit(2)


# -------------------- config substitution --------------------

CONFIG_KEYS = ["sequence_dir", "output_path", "timestamps_path", "calib_path", "time_budget_s"]


def find_config_template(exp_dir: Path) -> Path:
    for ext in ("yaml", "yml", "toml", "json"):
        p = exp_dir / f"config_template.{ext}"
        if p.exists():
            return p
    raise FileNotFoundError(f"no config_template.{{yaml,yml,toml,json}} in {exp_dir}")


def materialize_config(template: Path, dest: Path, subs: dict[str, str | int]) -> None:
    text = template.read_text()
    for k in CONFIG_KEYS:
        placeholder = "{" + k + "}"
        if placeholder in text:
            text = text.replace(placeholder, str(subs[k]))
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text)


# -------------------- output validation --------------------

POSE_LINE_RE = re.compile(r"^\s*([-+0-9.eE]+\s+){11}[-+0-9.eE]+\s*$")


def validate_trajectory(path: Path, expected_n: int) -> tuple[bool, str]:
    if not path.exists():
        return False, "output file missing"
    lines = path.read_text().splitlines()
    if len(lines) != expected_n:
        return False, f"line count {len(lines)} != expected {expected_n}"
    for i, line in enumerate(lines):
        parts = line.split()
        if len(parts) != 12:
            return False, f"line {i + 1} has {len(parts)} tokens, expected 12"
        try:
            vals = [float(p) for p in parts]
        except ValueError:
            return False, f"line {i + 1} contains a non-float token"
        if not all(math.isfinite(v) for v in vals):
            return False, f"line {i + 1} contains non-finite value"
    return True, ""


# -------------------- sequence run --------------------

def count_lines(p: Path) -> int:
    n = 0
    with p.open() as f:
        for _ in f:
            n += 1
    return n


def write_penalty_trajectory(path: Path, n: int) -> None:
    """Write a trajectory that scores ~100% trans error: stays at origin."""
    # All-zero translation, identity rotation == zero motion when GT moves. Yields ~100%.
    path.parent.mkdir(parents=True, exist_ok=True)
    line = "1 0 0 0 0 1 0 0 0 0 1 0\n"
    path.write_text(line * n)


def run_one_sequence(exp_dir: Path, seq: str, out_dir: Path, cap_s: int,
                     threads: int, cpu_offset: int) -> dict[str, Any]:
    """Run one sequence under wall-clock + RAM caps, pinned to a CPU set."""
    seq_dir = DATA_SEQS / seq
    timestamps = seq_dir / "times.txt"
    calib = seq_dir / "calib.txt"
    if not timestamps.exists():
        return {"seq": seq, "status": "no_data", "error": f"missing {timestamps}", "wall_s": 0.0,
                "peak_rss_kb": 0, "expected_n": 0}
    expected_n = count_lines(timestamps)

    out_path = out_dir / f"{seq}.txt"
    cfg_path = TMP_CONFIGS / exp_dir.name / f"config_{seq}.yaml"

    materialize_config(
        find_config_template(exp_dir),
        cfg_path,
        {
            "sequence_dir": str(seq_dir),
            "output_path": str(out_path),
            "timestamps_path": str(timestamps),
            "calib_path": str(calib),
            "time_budget_s": cap_s,
        },
    )

    cpu_list = ",".join(str(cpu_offset + i) for i in range(threads))
    # Use /usr/bin/time -v to capture peak RSS.
    inner = (
        f"ulimit -v {RAM_CAP_KB}; "
        f"exec /usr/bin/time -v -o {out_dir}/{seq}.time "
        f"./slam {cfg_path}"
    )
    cmd = ["taskset", "-c", cpu_list, "timeout", "--kill-after=10", str(cap_s),
           "bash", "-c", inner]

    log_path = RUN_LOGS / exp_dir.name / f"{seq}.log"
    start = time.monotonic()
    rc, out = run_capture(cmd, cwd=exp_dir, log_path=log_path, timeout=cap_s + 30)
    wall = time.monotonic() - start

    peak_rss_kb = 0
    time_file = out_dir / f"{seq}.time"
    if time_file.exists():
        for line in time_file.read_text().splitlines():
            m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", line)
            if m:
                peak_rss_kb = int(m.group(1))
                break

    if rc == 124:
        status = "timeout"
        write_penalty_trajectory(out_path, expected_n)
    elif rc != 0:
        status = "crash"
        write_penalty_trajectory(out_path, expected_n)
    else:
        ok, msg = validate_trajectory(out_path, expected_n)
        if ok:
            status = "ok"
        else:
            status = "invalid"
            (log_path.with_suffix(".invalid.log")).write_text(msg + "\n")
            write_penalty_trajectory(out_path, expected_n)

    return {
        "seq": seq,
        "status": status,
        "wall_s": wall,
        "peak_rss_kb": peak_rss_kb,
        "expected_n": expected_n,
        "output_path": str(out_path),
    }


def run_sequences_parallel(exp_dir: Path, seqs: list[str], out_dir: Path, cap_s: int,
                           threads: int) -> list[dict[str, Any]]:
    """Run sequences in parallel up to floor(N_cores / threads), pinning each to a CPU set."""
    cores = n_cores()
    slots = max(1, cores // max(1, threads))
    slots = min(slots, len(seqs))
    # Assign each task to a distinct CPU offset cyclically. With slots <= cores // threads,
    # taskset sets don't overlap.
    results: list[dict[str, Any]] = []
    with ThreadPoolExecutor(max_workers=slots) as ex:
        in_flight: dict[Any, tuple[str, int]] = {}
        next_offset_cycle = [i * threads for i in range(slots)]
        cursor = 0
        for seq in seqs:
            cpu_offset = next_offset_cycle[cursor % slots]
            cursor += 1
            fut = ex.submit(run_one_sequence, exp_dir, seq, out_dir, cap_s, threads, cpu_offset)
            in_flight[fut] = (seq, cpu_offset)
        for fut in as_completed(in_flight):
            results.append(fut.result())
    # Sort by sequence id for stable downstream behavior.
    results.sort(key=lambda r: r["seq"])
    return results


# -------------------- evaluation --------------------

def evaluate(pred_dir: Path, seqs: list[str]) -> dict[str, dict[str, float]]:
    """Invoke vendored devkit, parse JSON output. Returns {seq: {trans_pct, rot_deg_per_m, ...}}."""
    cmd = [str(DEVKIT), str(DATA_POSES), str(pred_dir)] + seqs
    rc, out = run_capture(cmd, log_path=RUN_LOGS / "last_eval.log")
    if rc != 0:
        raise RuntimeError(f"evaluator failed (rc={rc}): see run_logs/last_eval.log")
    result: dict[str, dict[str, float]] = {}
    for line in out.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        obj = json.loads(line)
        result[obj["seq"]] = {
            "trans_pct": float(obj["trans_pct"]),
            "rot_deg_per_m": float(obj["rot_deg_per_m"]),
            "n_subtraj": int(obj["n_subtraj"]),
            "length_m": float(obj["length_m"]),
        }
    return result


# -------------------- determinism check --------------------

def trajectories_identical(dir_a: Path, dir_b: Path, seqs: list[str]) -> bool:
    for seq in seqs:
        a = dir_a / f"{seq}.txt"
        b = dir_b / f"{seq}.txt"
        if not a.exists() or not b.exists():
            return False
        if a.read_bytes() != b.read_bytes():
            return False
    return True


# -------------------- top-level: run experiment --------------------

def current_best_dev_trans(rows: list[dict[str, str]]) -> float | None:
    best = None
    for r in rows:
        v = r.get("dev_trans_pct", "")
        if not v or r.get("status") not in ("keep", "reject"):
            # Only experiments with usable dev metrics count.
            pass
        try:
            f = float(v) if v else None
        except ValueError:
            f = None
        if f is None or not math.isfinite(f):
            continue
        if best is None or f < best:
            best = f
    return best


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("exp_dir", type=Path)
    ap.add_argument("--full", action="store_true",
                    help="Run the full set {00..10} instead of the dev set {00, 05, 07}.")
    args = ap.parse_args()

    exp_dir: Path = args.exp_dir.resolve()
    if not exp_dir.is_dir():
        sys.stderr.write(f"experiment dir not found: {exp_dir}\n")
        return 1

    exp_name = exp_dir.name
    is_full = args.full
    seqs = FULL_SEQS if is_full else DEV_SEQS
    cap_s = CAP_FULL_S if is_full else CAP_DEV_S

    # Parse meta.yaml.
    meta_path = exp_dir / "meta.yaml"
    if not meta_path.exists():
        sys.stderr.write(f"missing meta.yaml in {exp_dir}\n")
        return 1
    meta = parse_simple_yaml(meta_path)
    threads = int(meta.get("threads", 1))
    if not meta.get("deterministic", True):
        sys.stderr.write("meta.yaml says deterministic: false — not allowed\n")
        return 1

    ensure_devkit()

    # Build.
    print(f"[{exp_name}] building...", flush=True)
    build_ok, build_log = build_experiment(exp_dir)
    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    if not build_ok:
        print(f"[{exp_name}] BUILD FAIL → see {build_log}", flush=True)
        row = {
            "exp": exp_name,
            "parent": str(meta.get("parent") or ""),
            "timestamp": timestamp,
            "language": str(meta.get("language", "")),
            "status": "build_fail",
            "description": str(meta.get("description", "")),
        }
        upsert_results_row(row)
        return 0

    # Run sequences.
    slam_bin = exp_dir / "slam"
    if not slam_bin.exists():
        print(f"[{exp_name}] build.sh did not produce ./slam", flush=True)
        row = {
            "exp": exp_name, "parent": str(meta.get("parent") or ""),
            "timestamp": timestamp, "language": str(meta.get("language", "")),
            "status": "build_fail",
            "description": str(meta.get("description", "")),
        }
        upsert_results_row(row)
        return 0

    out_dir = exp_dir / ("preds_full" if is_full else "preds_dev")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir()

    print(f"[{exp_name}] running {'full' if is_full else 'dev'} set: {seqs}", flush=True)
    t0 = time.monotonic()
    seq_results = run_sequences_parallel(exp_dir, seqs, out_dir, cap_s, threads)
    wall_total = time.monotonic() - t0

    any_failure = any(r["status"] != "ok" for r in seq_results)
    peak_rss_mb = max((r["peak_rss_kb"] for r in seq_results), default=0) // 1024

    # Evaluate.
    metrics = evaluate(out_dir, seqs)
    agg = metrics.get("AGG", {"trans_pct": float("nan"), "rot_deg_per_m": float("nan")})

    # Build row.
    rows_existing = read_results_rows()
    best_dev = current_best_dev_trans(rows_existing)

    row: dict[str, str] = {
        "exp": exp_name,
        "parent": str(meta.get("parent") or ""),
        "timestamp": timestamp,
        "language": str(meta.get("language", "")),
        "description": str(meta.get("description", "")),
        "peak_rss_mb": str(peak_rss_mb),
    }

    if is_full:
        per_seq = "/".join(f"{metrics[s]['trans_pct']:.3f}" for s in FULL_SEQS if s in metrics)
        row.update({
            "full_trans_pct": f"{agg['trans_pct']:.4f}",
            "full_rot_deg_per_m": f"{agg['rot_deg_per_m']:.6f}",
            "full_per_seq": per_seq,
            "full_wall_s": f"{wall_total:.1f}",
            "status": "partial" if any_failure else "keep",
        })
        print(f"[{exp_name}] FULL agg trans={agg['trans_pct']:.4f}% rot={agg['rot_deg_per_m']:.6f}deg/m"
              f" wall={wall_total:.1f}s any_failure={any_failure}", flush=True)
        upsert_results_row(row)
        return 0

    # Dev path.
    row["dev_trans_pct"] = f"{agg['trans_pct']:.4f}"
    row["dev_rot_deg_per_m"] = f"{agg['rot_deg_per_m']:.6f}"
    for s in DEV_SEQS:
        if s in metrics:
            row[f"dev_seq_{s}"] = f"{metrics[s]['trans_pct']:.3f}"
    row["dev_wall_s"] = f"{wall_total:.1f}"

    if any_failure:
        row["status"] = "partial"
        upsert_results_row(row)
        print(f"[{exp_name}] DEV partial (some seq failed); trans={agg['trans_pct']:.4f}%", flush=True)
        return 0

    # Determinism check ONLY when this is a keep candidate.
    is_keep_candidate = best_dev is None or agg["trans_pct"] < best_dev
    if is_keep_candidate:
        print(f"[{exp_name}] keep-candidate; re-running dev for determinism check...", flush=True)
        out_dir_2 = exp_dir / "preds_dev_recheck"
        if out_dir_2.exists():
            shutil.rmtree(out_dir_2)
        out_dir_2.mkdir()
        seq_results_2 = run_sequences_parallel(exp_dir, seqs, out_dir_2, cap_s, threads)
        any_failure_2 = any(r["status"] != "ok" for r in seq_results_2)
        if any_failure_2 or not trajectories_identical(out_dir, out_dir_2, DEV_SEQS):
            row["status"] = "flaky"
            upsert_results_row(row)
            print(f"[{exp_name}] FLAKY (non-deterministic across re-run); rejected.", flush=True)
            return 0
        row["status"] = "keep"
    else:
        row["status"] = "reject"

    upsert_results_row(row)
    print(f"[{exp_name}] DEV {row['status']}; trans={agg['trans_pct']:.4f}% "
          f"(best={best_dev if best_dev is not None else 'NA'})", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
