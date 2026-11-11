# Parallel Aho–Corasick Matching

Portable research source code for comparing shared and private Aho–Corasick automata under OpenMP and single-host MPI. The same C sources support Linux and Windows. The project counts overlapping byte-pattern matches; it is not a regex engine and it is not a distributed multi-host benchmark.

## Repository layout

```text
src/                 C implementations and platform layer
scripts/generate.py  deterministic synthetic input generator
tests/               correctness smoke tests
Makefile             Linux/Unix build
CMakeLists.txt       cross-platform build description
docs/                experiment notes and result interpretation
```

Executables:

- `ac_bench`: serial, OpenMP shared, and OpenMP private layouts
- `ac_worker_diagnostic`: worker ownership and timing diagnostics
- `ac_bench_mpi`: MPI baseline (requires MPI)
- `ac_mpi_layout`: replicated, shared-input, and shared-input/shared-DFA layouts

## Linux build and test

Install GCC, OpenMP, Python 3, and optionally Open MPI. Then:

```bash
make all
make mpi                 # optional MPI targets
make data
make test
```

The equivalent helper is `bash scripts/build_linux.sh`.

The smoke test uses a small generated input and verifies that serial, shared, and private modes produce identical match counts. It also verifies that malformed hexadecimal input is rejected. It is a correctness test, not a performance result.

For a one-command run on a normal Debian/Ubuntu system, execute:

```bash
./START_HERE.sh
```

The launcher checks for Python, GCC, Make, OpenMP, and Open MPI, asks the package manager to install missing Linux dependencies, builds the programs, runs the correctness checks, generates the campaign inputs, and writes raw results plus an evidence ZIP under `results/`. Set `REPEATS=10` for ten timing repetitions. Use the smaller `python3 scripts/run_campaign.py --repeats 3` during a quick trial.

Example run:

```bash
build/bin/ac_bench serial 1 0 block data/demo/uniform.bin data/demo/patterns.hex
OMP_NUM_THREADS=4 build/bin/ac_bench shared 4 262144 dynamic data/demo/uniform.bin data/demo/patterns.hex
```

MPI examples after `make mpi`:

```bash
mpirun -n 4 build/bin/ac_bench_mpi mpi 4 0 block data/demo/uniform.bin data/demo/patterns.hex
AC_MPI_LAYOUT=shared_all mpirun -n 4 build/bin/ac_mpi_layout mpi 4 0 block data/demo/uniform.bin data/demo/patterns.hex
```

Use `AC_WARMUP_SCANS=2` for the paper's two untimed warm-up scans. Timing excludes input reading and automaton construction.

## Windows build

Use a 64-bit MinGW-w64 GCC toolchain with OpenMP. For MPI targets, install Microsoft MPI and its SDK, then expose `mpicc` or compile with the SDK include/library paths. The same source files are used; `native_platform.h` selects Windows timing, affinity, and memory APIs when `_WIN32` is defined.

OpenMP-only example from a MinGW shell:

```powershell
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -Isrc src/ac_bench.c -o ac_bench.exe
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp -Isrc src/ac_worker_diagnostic.c -o ac_worker_diagnostic.exe
```

PowerShell users can run `.\scripts\build_windows.ps1`; pass `-CC` and `-MPICC` when the toolchain is not on `PATH`.

For a guided Windows run, use `.\START_HERE.ps1 -InstallDependencies -Full`. It uses `winget` for Python and MSYS2, builds the OpenMP programs, runs correctness checks, and writes an evidence ZIP. Microsoft MPI is used when an `mpicc` toolchain is already installed; Windows administrator policy and MPI installation are kept explicit rather than bypassed.

The earlier native Windows ZIP contains prebuilt executables and the Microsoft MPI installation workflow. This repository intentionally contains source and build instructions rather than redistributing third-party installers or binaries.

## Input format

`patterns.hex` contains one non-empty even-length hexadecimal line per pattern. Duplicate lines remain separate signatures. Input text is read as raw bytes, so NUL and non-ASCII bytes are valid. Overlapping matches and matches crossing worker boundaries are counted once by assigning each match to its ending position.

## Reproducibility and interpretation

Use fixed seeds and save the generated `manifest.json`, compiler version, OS, CPU placement, raw JSON output, and command line. Report serial baseline, speedup `T_serial/T_parallel`, and efficiency `speedup/p × 100%` together with the number of workers. Technical repetitions are not independent machines. Results from one laptop or one operating system must not be presented as universal hardware laws.

The MPI layout program is restricted to one shared-memory host. Linux PSS and Windows working-set/private-commit values are different metrics and must not be pooled. Cache or memory-bandwidth causes require hardware-counter evidence; timing alone does not establish mechanism.

## License and research responsibility

Add the license required by your institution or venue before publishing this repository. Keep generated data, raw measurements, and the exact command log with each result. Verify all code, results, citations, and author declarations before submission.
