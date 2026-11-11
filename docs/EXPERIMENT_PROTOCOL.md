# Experiment protocol

1. Record OS, compiler, OpenMP runtime, MPI implementation, CPU topology, power state, and available memory.
2. Generate deterministic inputs with `scripts/generate.py`; retain `manifest.json` and file hashes.
3. Run the correctness smoke test before timing.
4. Use two untimed warm-up scans in the same process (`AC_WARMUP_SCANS=2`).
5. Time serial, shared, and private modes with the same input and pattern files. Keep construction, file I/O, and copying outside the scan timer.
6. Repeat each configuration, retaining every observation, including slow runs. Report mean, sample SD, median, and CV.
7. Compute speedup against the stated serial baseline and efficiency using the actual worker count.
8. Keep OpenMP, MPI, operating systems, machines, and diagnostic runs in separate result groups.

Do not call a smoke run a completed campaign. Do not combine Windows working-set measurements with Linux PSS, and do not infer cache causation from elapsed time alone.
