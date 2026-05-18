#!/usr/bin/env bash
# Minimal frame-to-frame ICP baseline. No external deps.
set -euo pipefail
cd "$(dirname "$0")"
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic main.cpp -o slam
