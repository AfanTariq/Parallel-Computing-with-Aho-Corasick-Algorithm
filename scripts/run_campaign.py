#!/usr/bin/env python3
"""Portable benchmark orchestrator. It keeps raw JSON output and creates an evidence ZIP."""
from __future__ import annotations
import argparse, csv, hashlib, json, os, platform, shutil, subprocess, sys, time, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def call(cmd, *, env=None, timeout=900):
    p = subprocess.run([str(x) for x in cmd], cwd=ROOT, env=env, text=True,
                       capture_output=True, timeout=timeout)
    if p.returncode:
        raise RuntimeError(f"Command failed ({p.returncode}): {' '.join(map(str, cmd))}\n{p.stderr[-4000:]}")
    return p.stdout

def exe(name):
    p = ROOT / "build" / "bin" / name
    q = p.with_suffix(".exe")
    if q.exists(): return q
    if p.exists(): return p
    raise FileNotFoundError(f"Missing {name}; run the platform launcher first.")

def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""): h.update(block)
    return h.hexdigest()

def run_json(binary, args, env):
    out = call([binary, *args], env=env)
    return json.loads(out)

def generate(out, size, patterns, seed):
    call([sys.executable, ROOT/"scripts"/"generate.py", "--out", out,
          "--bytes", size, "--patterns", patterns, "--seed", seed])

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=ROOT/"results"/time.strftime("run_%Y%m%d_%H%M%S"))
    ap.add_argument("--repeats", type=int, default=3, help="timed repetitions (10 for the paper campaign)")
    ap.add_argument("--full", action="store_true", help="use the larger 100 MiB, 1k/10k, three-seed campaign")
    ns = ap.parse_args(); ns.out = ns.out.resolve(); ns.out.mkdir(parents=True, exist_ok=False)
    if ns.repeats < 1: ap.error("--repeats must be positive")
    env = os.environ.copy(); env.update({"AC_WARMUP_SCANS":"2", "OMP_DYNAMIC":"false"})
    call([sys.executable, ROOT/"tests"/"test_smoke.py", "--bin-dir", ROOT/"build"/"bin"], env=env)
    if ns.full:
        sizes = [(104857600, 1000), (104857600, 10000)]; seeds = [101, 202, 303]
    else:
        sizes = [(4*1024*1024, 1000)]; seeds = [101]
    workers = sorted(set([1, max(1, (os.cpu_count() or 2)//2), min(4, os.cpu_count() or 2)]))
    rows=[]; data_root=ns.out/"data"; data_root.mkdir()
    for size, count in sizes:
        for seed in seeds:
            data=data_root/f"{size}_{count}_seed{seed}"; generate(data,size,count,seed)
            text=data/"uniform.bin"; patterns=data/"patterns.hex"
            serial=run_json(exe("ac_bench"), ["serial","1","0","block",text,patterns], env)
            for mode in ("shared","private"):
                for w in workers:
                    for schedule, chunk in (("block",0),("dynamic",262144)):
                        for rep in range(1, ns.repeats+1):
                            x=run_json(exe("ac_bench"), [mode,w,chunk,schedule,text,patterns], env)
                            if x["matches"] != serial["matches"]: raise RuntimeError("count mismatch")
                            x.update(dataset=str(data.name),patterns=count,seed=seed,repeat=rep,serial_seconds=serial["seconds"])
                            rows.append(x)
    (ns.out/"raw.jsonl").write_text("\n".join(json.dumps(x) for x in rows)+"\n", encoding="utf-8")
    with (ns.out/"summary.csv").open("w", newline="", encoding="utf-8") as f:
        fields=["dataset","mode","workers_actual","schedule","chunk_bytes","patterns","repeat","seconds","serial_seconds","matches"]
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows({k:x.get(k) for k in fields} for x in rows)
    meta={"status":"PASS","platform":platform.platform(),"python":sys.version,"repeats":ns.repeats,"full":ns.full,"rows":len(rows),"workers":workers,"files":{"raw.jsonl":sha(ns.out/"raw.jsonl"),"summary.csv":sha(ns.out/"summary.csv")}}
    (ns.out/"COMPLETE.json").write_text(json.dumps(meta,indent=2)+"\n",encoding="utf-8")
    evidence=ns.out.with_name(ns.out.name+"_evidence.zip")
    with zipfile.ZipFile(evidence,"w",zipfile.ZIP_DEFLATED) as z:
        for p in sorted(ns.out.rglob("*")):
            if p.is_file(): z.write(p, "results/"+str(p.relative_to(ns.out)).replace(os.sep,"/"))
        for p in [ROOT/"README.md",ROOT/"Makefile",ROOT/"CMakeLists.txt"]:
            z.write(p,"source/"+p.name)
    (ns.out/"EVIDENCE_ZIP.txt").write_text(str(evidence)+"\n"+sha(evidence)+"\n",encoding="utf-8")
    print(f"COMPLETE: {ns.out}\nEVIDENCE: {evidence}")

if __name__ == "__main__":
    main()
