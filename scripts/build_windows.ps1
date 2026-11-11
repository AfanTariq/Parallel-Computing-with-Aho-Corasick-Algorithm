[CmdletBinding()]
param([string]$CC = "gcc", [string]$MPICC = "mpicc")
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path build\bin | Out-Null
& $CC -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -Isrc src\ac_bench.c -o build\bin\ac_bench.exe
if ($LASTEXITCODE -ne 0) { throw "OpenMP build failed" }
& $CC -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -Isrc src\ac_worker_diagnostic.c -o build\bin\ac_worker_diagnostic.exe
if ($LASTEXITCODE -ne 0) { throw "Diagnostic build failed" }
if (Get-Command $MPICC -ErrorAction SilentlyContinue) {
  & $MPICC -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -DWITH_MPI -Isrc src\ac_bench.c -o build\bin\ac_bench_mpi.exe
  if ($LASTEXITCODE -ne 0) { throw "MPI baseline build failed" }
  & $MPICC -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -DWITH_MPI -Isrc src\ac_mpi_layout.c -o build\bin\ac_mpi_layout.exe
  if ($LASTEXITCODE -ne 0) { throw "MPI layout build failed" }
} else {
  Write-Warning "mpicc not found; OpenMP targets built, MPI targets skipped."
}
Write-Host "Build completed. Run: python tests\test_smoke.py --bin-dir build\bin"
