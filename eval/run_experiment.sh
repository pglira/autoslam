#!/usr/bin/env bash
# autoslam — invoke harness for one experiment.
#
# Usage:
#   bash eval/run_experiment.sh experiments/expNNNN_<slug>           # dev set
#   bash eval/run_experiment.sh experiments/expNNNN_<slug> --full    # full set
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "${ROOT}/eval/_harness.py" "$@"
