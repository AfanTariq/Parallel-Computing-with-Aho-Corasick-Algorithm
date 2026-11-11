#!/usr/bin/env python3
"""Deterministic synthetic inputs. Hex patterns preserve arbitrary bytes and duplicates."""
import argparse, hashlib, json, random
from pathlib import Path

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--out',type=Path,default=Path('data/demo'))
    p.add_argument('--bytes',type=int,default=4*1024*1024)
    p.add_argument('--patterns',type=int,default=1000)
    p.add_argument('--seed',type=int,default=20261027)
    p.add_argument('--min-length',type=int,default=6)
    p.add_argument('--max-length',type=int,default=16)
    a=p.parse_args()
    if not (0<a.min_length<=a.max_length<=a.bytes and a.patterns>0):
        p.error('need 0 < min-length <= max-length <= bytes and patterns > 0')
    a.out.mkdir(parents=True,exist_ok=True)
    r=random.Random(a.seed); alphabet=b'abcdefghijklmnopqrstuvwxyz '
    uniform=bytearray(alphabet[i%len(alphabet)] for i in r.randbytes(a.bytes))
    patterns=[]
    for _ in range(a.patterns):
        length=r.randint(a.min_length,a.max_length); start=r.randrange(a.bytes-length+1)
        patterns.append(bytes(uniform[start:start+length]))
    # Same patterns; original uniform input remains available and guaranteed to contain each.
    prefix=(b''.join(patterns)*((a.bytes//4)//sum(map(len,patterns))+1))[:a.bytes//4]
    skewed=prefix+uniform[len(prefix):]
    (a.out/'uniform.bin').write_bytes(uniform)
    (a.out/'skewed.bin').write_bytes(skewed)
    (a.out/'patterns.hex').write_bytes(''.join(x.hex()+'\n' for x in patterns).encode('ascii'))
    manifest={'schema':1,'seed':a.seed,'generator':'scripts/generate.py','text_bytes':a.bytes,
              'patterns_including_duplicates':len(patterns),'distinct_patterns':len(set(patterns)),
              'min_pattern_bytes':min(map(len,patterns)),'max_pattern_bytes':max(map(len,patterns)),
              'duplicate_semantics':'Each pattern line is a separate signature; all overlaps count.',
              'uniform_minimum_matches':len(patterns),'skew_prefix_bytes':len(prefix),
              'alphabet_generation':'random.Random.randbytes values modulo 27; small modulo bias is intentional and documented.',
              'files':{name:sha(a.out/name) for name in ['uniform.bin','skewed.bin','patterns.hex']}}
    (a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(a.out/'manifest.json')
if __name__=='__main__': main()
