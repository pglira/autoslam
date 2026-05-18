#!/usr/bin/env bash
# Build the vendored KITTI evaluator. Idempotent.
set -euo pipefail
cd "$(dirname "$0")"
if [[ -x evaluate_odometry && evaluate_odometry -nt evaluate_odometry.cpp ]]; then
    exit 0
fi
g++ -std=c++17 -O2 -Wall -Wextra evaluate_odometry.cpp -o evaluate_odometry
