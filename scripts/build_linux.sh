#!/usr/bin/env bash
set -euo pipefail
make all
if command -v mpicc >/dev/null 2>&1; then
  make mpi
else
  echo "mpicc not found; OpenMP targets built, MPI targets skipped." >&2
fi
