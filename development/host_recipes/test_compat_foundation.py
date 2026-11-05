from pathlib import Path
import subprocess,tempfile,os
R=Path(__file__).resolve().parents[1]/'compat_candidate'
for p in R.glob('*.sh'):subprocess.run(['sh','-n',str(p)],check=True)
rows=[x.split() for x in (R/'rom-profiles.tsv').read_text().splitlines() if not x.startswith('#')]
with tempfile.TemporaryDirectory() as t:
 t=Path(t);(t/'rom-profiles.tsv').write_bytes((R/'rom-profiles.tsv').read_bytes())
 for row in rows:
  script='MODDIR='+str(t)+'\n. '+str(R/'rom-profile.sh')+'\noc_profile_find "$1" "$2" "$3" "$4"\n'
  assert subprocess.run(['sh','-c',script,'test',*row[1:5]]).returncode==0
  for k in range(4):
   q=row[1:5].copy();q[k]='0'*64;assert subprocess.run(['sh','-c',script,'test',*q]).returncode!=0
 # No Cartesian mix-and-match acceptance.
 for a in rows:
  for b in rows:
   q=a[1:3]+b[3:5]
   rc=subprocess.run(['sh','-c',script,'test',*q]).returncode
   assert (rc==0)==any(x[1:5]==q for x in rows)
 # Exercise actual boot functions with virtual boot-id and boot-complete reads.
 src=(R/'boot-guard.sh').read_text().replace('/proc/sys/kernel/random/boot_id',str(t/'bootid'))
 (t/'guard.sh').write_text(src);(t/'bootid').write_text('boot-one\n')
 code='MODDIR='+str(t)+'\ngetprop(){ echo "$COMPLETE"; }\n. '+str(t/'guard.sh')+'\n'
 def run(op,complete='0'):return subprocess.run(['sh','-c',code+op],env={**os.environ,'COMPLETE':complete}).returncode
 assert run('oc_boot_guard_enter')==0
 assert run('oc_boot_guard_confirm')!=0
 assert run('oc_boot_guard_enter')==0
 assert run('oc_boot_guard_confirm','1')==0
 assert not (t/'.boot-pending').exists()
 assert run('oc_boot_guard_enter')==0
 (t/'bootid').write_text('boot-two\n')
 assert run('oc_boot_guard_enter')!=0 and (t/'disable').exists()
 assert run('oc_boot_guard_enter')!=0
print('PASS5profiletuples,20single-fieldcorruptions,25cross-paircombinations,boot success/same-boot retry/incompleteboot/nextboot disable; shell syntax')
