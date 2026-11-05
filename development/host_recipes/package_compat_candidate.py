#!/usr/bin/env python3
"""Build a disabled candidate ZIP; never installs or mutates retained staging."""
from pathlib import Path
import subprocess,hashlib,json,shutil,zipfile,os,stat
P=Path(__file__).resolve().parents[1];B=Path('/path/to/local-workspace/fresh_f14');OUT=B/'compat_builds/occam_f14_fresh';SRC=B/'module_stage/occam_f14_fresh';F=P/'compat_candidate/module_overrides'
if OUT.exists():raise SystemExit('Refusing to overwrite existing candidate tree')
OUT.parent.mkdir(exist_ok=True);subprocess.run(['cp','-a','--reflink=auto',str(SRC),str(OUT)],check=True)
for p in F.iterdir():
 if p.is_file() and p.suffix!='.py':shutil.copy2(p,OUT/p.name)
# Remove all instance-specific session/profile state; package only source payload.
for n in ['.root-backend','.runtime-rom-profile','.boot-pending','.boot-pending.next']:
 (OUT/n).unlink(missing_ok=True)
(OUT/'system/priv-app/OppoCamera/fresh_oem_bootstrap/active_hal_profile.json').unlink(missing_ok=True)
assert not (OUT/'.stdcpp-transaction').exists()
for p in (OUT/'opencl_runtime').iterdir():
 if not p.is_file():raise RuntimeError('Unexpected GPU staging entry')
 p.unlink()
assert not (OUT/'sepolicy.rule').exists()
# Old fallback helper isn't used; remove it from this candidate to avoid a
# Cyborg-only manual route surviving in an otherwise multi-profile package.
for n in ['provider_hw','provider_original.sha256']:
 p=OUT/n
 if p.is_dir():shutil.rmtree(p)
 elif p.exists():p.unlink()
(OUT/'module.prop').write_text('id=occam_f14_fresh\nname=OcCam F14 compatibility candidate (DISABLED)\nversion=0.7-host-candidate\nversionCode=7\nauthor=Arka Roy Chowdhury\ndescription=Experimental five-profile KSU/Magisk package. Requires OcCam Fresh 0.113. Unbooted profiles remain unqualified. Manual HAL Action only.\n')
shutil.copy2(P/'compat_candidate/RECOVERY.md',OUT/'RECOVERY.md')
shutil.copy2(P/'compat_candidate/STATUS.md',OUT/'QUALIFICATION.md')
for d in ['camera_power']:
 for n in ['session.env']:
  assert not (OUT/d/n).exists(),'Retained staging unexpectedly contains live session state'
for p in OUT.glob('*.sh'):subprocess.run(['sh','-n',str(p)],check=True)
# Standard Magisk installer bootstrap; KernelSU's manager supplies its own
# module installer and reads customize.sh. No root binary bundled.
meta=OUT/'META-INF/com/google/android';meta.mkdir(parents=True,exist_ok=True)
(meta/'updater-script').write_text('#MAGISK\n')
shutil.copy2(B/'analysis/compat_review_20261105/module_installer.official.sh',meta/'update-binary')
os.chmod(meta/'update-binary',0o755)
rows=[]
for p in sorted(OUT.rglob('*')):
 if p.is_file() and not p.is_symlink() and p.name!='payload.sha256':rows.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(OUT)))
(OUT/'payload.sha256').write_text('\n'.join(rows)+'\n')
# Verify original donor identities before archiving.
assert hashlib.sha256((OUT/'system/priv-app/OppoCamera/OppoCamera.apk').read_bytes()).hexdigest()=='3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5'
assert hashlib.sha256((OUT/'f14_runtime/provider_hw/occam_f14_stack/liboc_f14_selector.so').read_bytes()).hexdigest()=='3eb0986349165e98c855c712efe6445b84515b0f1eaea4f752dd954ac2d928bd'
subprocess.run(['sha256sum','-c','payload.sha256'],cwd=OUT,stdout=subprocess.DEVNULL,check=True)
Z=B/'compat_builds/occam-f14-0.7-five-profile-KSU-Magisk-EXPERIMENTAL-DISABLED.zip'
with zipfile.ZipFile(Z,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=5) as z:
 for p in sorted(OUT.rglob('*')):
  rel=str(p.relative_to(OUT))
  if p.is_symlink():
   i=zipfile.ZipInfo(rel);i.create_system=3;i.external_attr=(stat.S_IFLNK|0o777)<<16;z.writestr(i,os.readlink(p))
  elif p.is_file():z.write(p,rel)
with zipfile.ZipFile(Z) as z:
 assert z.testzip() is None
 assert 'disable' in z.namelist() and 'skip_mount' in z.namelist() and 'sepolicy.rule' not in z.namelist()
 assert not any(x.startswith('opencl_runtime/') and not x.endswith('/') for x in z.namelist())
 # Preserve empty runtime dir needed for validated per-ROM materialization.
with zipfile.ZipFile(Z,'a') as z:z.writestr('opencl_runtime/','')
result={'zip':str(Z),'sha256':hashlib.sha256(Z.read_bytes()).hexdigest(),'bytes':Z.stat().st_size,'runtime_tested':False,'installed':False,'requires_hook':113}
(B/'compat_builds/build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
