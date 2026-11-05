"""Host-only regression checks. No device, mounts or policy writes."""
from pathlib import Path
import tempfile,subprocess,os,json,hashlib,zipfile,argparse
r=Path(__file__).resolve().parents[1];m=r/'module'
for p in m.glob('*.sh'):subprocess.run(['sh','-n',str(p)],check=True)
s=(m/'early-mounts.sh').read_text();functions=s[:s.rindex('case "${1:-mounts}"')]
with tempfile.TemporaryDirectory() as raw:
 t=Path(raw);rd=t/'readlink';rd.write_text('#!/bin/sh\ncase "$2" in /product/etc/default-permissions) echo /system/product/etc/default-permissions;; *) echo "$2";; esac\n');rd.chmod(0o755)
 info=t/'mountinfo';script=t/'check.sh';script.write_text(functions+'\noc_simple_owns_overlay system/etc/default-permissions /product/etc/default-permissions\n')
 env={**os.environ,'PATH':str(t)+':'+os.environ['PATH'],'MODDIR':'/data/adb/modules/occam_f14_fresh','OC_SIMPLE_MOUNTINFO':str(info)}
 for target,source,expected in [('/system/product/etc/default-permissions','/data/adb/modules/occam_f14_fresh/system/etc/default-permissions',0),('/wrong','/data/adb/modules/occam_f14_fresh/system/etc/default-permissions',1),('/system/product/etc/default-permissions','/data/adb/modules/other/system/etc/default-permissions',1)]:
  info.write_text(f'1 2 0:1 / {target} rw - overlay overlay lowerdir={source}:/product/etc/default-permissions\n');assert subprocess.run(['sh',str(script)],env=env).returncode==expected
print('PASS canonical /product mount ownership and wrong-target/wrong-source refusal')
rows=[x.split() for x in (m/'early-mounts.tsv').read_text().splitlines() if x and not x.startswith('#')]
assert len(rows)==11 and all(len(x)==5 for x in rows)
assert {x[1] for x in rows if x[0]=='replace'}=={'f14_runtime/camera_config','f14_runtime/vendor64/camera'}
assert not any((m/n).exists() for n in ['action.sh','auto-start.sh','f14-runtime.sh','access-runtime.sh','provider-runtime.sh','disable'])
controller=(m/'post-fs-data.sh').read_text();assert 'a16_cyborg_live' in controller and 'oc_refuse' in controller
for name in ['post-fs-data.sh','early-mounts.sh','service.sh']:
 active='\n'.join(l for l in (m/name).read_text().splitlines() if not l.lstrip().startswith('#'))
 assert 'ctl.' not in active and 'sepolicy patch' not in active and 'oc_policy apply' not in active
rule=[l.strip() for l in (m/'sepolicy.rule').read_text().splitlines() if l and not l.startswith('#')]
assert len(rule)==3 and all(x.startswith('allow ') for x in rule)
print('PASS replacement-overlay rows, no legacy activation scripts, no late policy/provider controls')
p=argparse.ArgumentParser();p.add_argument('--assets',type=Path);a=p.parse_args()
if a.assets:
 d=json.loads((a.assets/'release-manifest.json').read_text());inv=json.loads((r/'docs/payload-inventory.json').read_text())['files']
 for row in d['files']:assert hashlib.sha256((a.assets/row['name']).read_bytes()).hexdigest()==row['sha256']
 zpath=next(a.assets.glob('occam-f14-module-*.zip'))
 with zipfile.ZipFile(zpath) as z:
  for row in inv:assert hashlib.sha256(z.read(row['path'])).hexdigest()==row['sha256'],row['path']
  for name in ['early-mounts.sh','early-mounts.tsv','post-fs-data.sh','service.sh','hook-preflight.sh','customize.sh','module.prop','sepolicy.rule']:assert z.read(name)==(m/name).read_bytes(),name
  assert 'test_front_portrait_full_size' not in '\n'.join(z.namelist())
  assert z.read('META-INF/com/google/android/update-binary')==z.read('LICENSES/module_installer.source.sh')
  assert hashlib.sha256(z.read('system/priv-app/OppoCamera/OppoCamera.apk')).hexdigest()=='3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5'
 hook=next(a.assets.glob('occam-fresh-hook-*.apk'));assert hashlib.sha256(hook.read_bytes()).hexdigest() in (m/'hook-preflight.sh').read_text()
 print('PASS complete ZIP inventory, matching public controls, original camera APK, paired hook, installer source/license')
