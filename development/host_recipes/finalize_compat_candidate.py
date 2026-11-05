"""Refresh candidate docs/checks/archive only; no retained staging/device edits."""
from pathlib import Path
import zipfile,hashlib,json,os,stat,subprocess
B=Path('/path/to/local-workspace/fresh_f14/compat_builds');T=B/'occam_f14_fresh';P=Path(__file__).resolve().parents[1]
assert (T/'module.prop').read_text().startswith('id=occam_f14_fresh\n')
assert '0.7-host-candidate' in (T/'module.prop').read_text()
(T/'QUALIFICATION.md').write_bytes((P/'compat_candidate/STATUS.md').read_bytes())
(T/'RECOVERY.md').write_bytes((P/'compat_candidate/RECOVERY.md').read_bytes())
rows=[]
for p in sorted(T.rglob('*')):
 if p.is_file() and not p.is_symlink() and p.name!='payload.sha256':rows.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(T)))
(T/'payload.sha256').write_text('\n'.join(rows)+'\n');subprocess.run(['sha256sum','-c','payload.sha256'],cwd=T,stdout=subprocess.DEVNULL,check=True)
Z=B/'occam-f14-0.7-five-profile-KSU-Magisk-EXPERIMENTAL-DISABLED.zip'
with zipfile.ZipFile(Z,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=5) as z:
 for p in sorted(T.rglob('*')):
  rel=str(p.relative_to(T))
  if p.is_symlink():
   i=zipfile.ZipInfo(rel);i.create_system=3;i.external_attr=(stat.S_IFLNK|0o777)<<16;z.writestr(i,os.readlink(p))
  elif p.is_file():z.write(p,rel)
 z.writestr('opencl_runtime/','')
with zipfile.ZipFile(Z) as z:
 assert z.testzip() is None
 assert 'disable' in z.namelist() and 'skip_mount' in z.namelist() and 'sepolicy.rule' not in z.namelist()
 assert z.read('system/priv-app/OppoCamera/OppoCamera.apk')==(T/'system/priv-app/OppoCamera/OppoCamera.apk').read_bytes()
j={'zip':str(Z),'sha256':hashlib.sha256(Z.read_bytes()).hexdigest(),'bytes':Z.stat().st_size,'runtime_tested':False,'installed':False,'requires_hook':113};(B/'build.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j))
