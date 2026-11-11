#!/usr/bin/env python3
"""Small correctness smoke test; it is not a performance campaign."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(exe: Path, args: list[str], env: dict[str, str]) -> dict:
    p = subprocess.run([str(exe), *args], text=True, capture_output=True, env=env)
    if p.returncode:
        raise RuntimeError(f"{exe.name} failed ({p.returncode}): {p.stderr}")
    return json.loads(p.stdout)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin-dir", type=Path, default=Path("build/bin"))
    ns = ap.parse_args()
    root = Path(__file__).resolve().parents[1]
    generator = root / "scripts" / "generate.py"
    with tempfile.TemporaryDirectory(prefix="aho_smoke_") as tmp:
        data = Path(tmp)
        subprocess.run([sys.executable, str(generator), "--out", str(data), "--bytes", "65536", "--patterns", "128", "--seed", "7"], check=True, capture_output=True)
        common = ["uniform.bin", "patterns.hex"]
        env = os.environ.copy()
        env.update({"AC_WARMUP_SCANS": "1", "OMP_NUM_THREADS": "2", "OMP_DYNAMIC": "false"})
        path_args = [str(data / common[0]), str(data / common[1])]
        serial = run(ns.bin_dir / "ac_bench", ["serial", "1", "0", "block", *path_args], env)
        for mode in ("shared", "private"):
            result = run(ns.bin_dir / "ac_bench", [mode, "2", "4096", "dynamic", *path_args], env)
            assert result["matches"] == serial["matches"], (mode, result, serial)
        invalid = data / "invalid.hex"
        invalid.write_text("zz\n", encoding="ascii")
        bad = subprocess.run([str(ns.bin_dir / "ac_bench"), "serial", "1", "0", "block", str(data / common[0]), str(invalid)], capture_output=True, text=True, env=env)
        assert bad.returncode != 0, "invalid pattern input was accepted"
    print("PASS: serial/shared/private correctness and invalid-input rejection")


if __name__ == "__main__":
    main()
