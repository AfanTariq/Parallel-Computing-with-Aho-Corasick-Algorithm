"""Development checks only; not research benchmark observations."""
import importlib.util,json,tempfile,zipfile,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native_runner',ROOT/'scripts/run_native.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def main():
 for n,want in [(2,2),(3,2),(4,4),(8,4)]:
  probe={'cores':[{'logical_cpus':[2*i,2*i+1]} for i in range(n)]};cpus,label=m.select_profile(probe);assert len(cpus)==want and len(set(cpus))==want
 try:m.select_profile({'cores':[{'logical_cpus':[0,1]}]})
 except RuntimeError:pass
 else:raise AssertionError('SMT siblings must not be treated as distinct physical cores')
 rows=[dict(experiment='test',dataset='uniform.bin',mode='shared',workers_actual=2,schedule='block',chunk_bytes=0,executable='ac_bench',mpi_layout='replicated',seconds=t) for t in [1.,2.,3.]]
 r=m.summarize(rows)[0];assert r['mean_seconds']==2 and r['sd_seconds']==1 and r['cv_percent']==50
 for n,expected in [(4,1240),(2,1000)]:
  c=m.Campaign(Path('.'),list(range(n)),'mpiexec.exe');assert 6*(1+2*len(c.workers))*2*10+(2+2*3*3)*2*10==expected
 with tempfile.TemporaryDirectory() as d:
  d=Path(d);out=d/'results/failed_run';out.mkdir(parents=True);(d/'src').mkdir();(d/'scripts').mkdir();(d/'reference_manifests').mkdir();(d/'data').mkdir();m.save(out/'FAILURE.json',{'status':'FAIL'});(d/'README_ENGLISH.md').write_text('retain this document');old=m.ROOT;m.ROOT=d
  try:
   z=m.export_evidence(out);assert z.name=='failed_run_evidence.zip'
   with zipfile.ZipFile(z) as f:
    assert f.testzip() is None and 'results/FAILURE.json' in f.namelist()
    for line in f.read('SHA256SUMS.txt').decode().splitlines():
     digest,name=line.split('  ',1);assert hashlib.sha256(f.read(name)).hexdigest()==digest
   assert (d/'README_ENGLISH.md').read_text()=='retain this document'
  finally:m.ROOT=old
 print(json.dumps({'status':'PASS','checks':['physical-core profiles and SMT exclusion','descriptive statistics','profile inventories','failure ZIP pathname, checksums and document preservation']}))
if __name__=='__main__':main()
