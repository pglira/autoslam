#!/usr/bin/env bash
# Build the identity reference. Idempotent.
set -euo pipefail
cd "$(dirname "$0")"
cc -O2 -std=c11 -Wall -Wextra slam.c -o slam
