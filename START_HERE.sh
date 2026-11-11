#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then SUDO=""; elif command -v sudo >/dev/null 2>&1; then SUDO=sudo; else SUDO=""; fi
missing=()
for c in python3 gcc make; do command -v "$c" >/dev/null 2>&1 || missing+=("$c"); done
if ((${#missing[@]})); then
  if command -v apt-get >/dev/null 2>&1; then
    echo "Installing missing Linux dependencies: ${missing[*]} (administrator permission may be requested)."
    $SUDO apt-get update
    $SUDO apt-get install -y python3 gcc make libomp-dev openmpi-bin libopenmpi-dev
  else
    echo "Missing: ${missing[*]}. Install Python 3, GCC, Make, and OpenMP, then rerun." >&2; exit 2
  fi
fi
if command -v mpicc >/dev/null 2>&1; then make all mpi; else make all; echo "mpicc not found; MPI build skipped."; fi
python3 scripts/run_campaign.py --full --repeats "${REPEATS:-3}"
