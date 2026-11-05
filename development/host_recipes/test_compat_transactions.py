from pathlib import Path
import tempfile,subprocess,hashlib
F=Path(__file__).resolve().parents[1]/'compat_candidate/module_overrides'
s=(F/'f14-runtime.sh').read_text();functions=s[s.index('# Transaction backups'):s.index('check_payload() {')]
with tempfile.TemporaryDirectory() as tmp:
 t=Path(tmp);r=t/'runtime';(r/'vendor64').mkdir(parents=True);old=b'old ROM facade';new=b'new attested ROM facade';want=hashlib.sha256(new).hexdigest();lib=r/'vendor64/libstdc++.so';checks=r/'files.sha256';src=t/'source';src.write_bytes(new)
 checks_orig=hashlib.sha256(old).hexdigest()+'  vendor64/libstdc++.so\n'
 def reset():
  lib.write_bytes(old);checks.write_text(checks_orig)
 def run(body):
  code='MODDIR='+str(t)+'\nR='+str(r)+'\nOC_WANT_STDC='+want+'\n'+'''hash_is(){ got=$(sha256sum "$2"); [ "${got%% *}" = "$1" ]; }
any_mount(){ return 1; }
'''+functions.replace('/vendor/lib64/libstdc++_vendor.so',str(src))+'\n'+body
  return subprocess.run(['sh','-c',code],capture_output=True,text=True)
 reset();q=run('oc_stdc_prepare && oc_stdc_restore');assert q.returncode==0,q.stderr;assert lib.read_bytes()==old and checks.read_text()==checks_orig
 reset();q=run('oc_stdc_prepare && oc_stdc_commit');assert q.returncode==0,q.stderr;assert lib.read_bytes()==new and want in checks.read_text() and not (t/'.stdcpp-transaction').exists()
 reset();q=run('''mv(){ case "$1" in */files.sha256.next) return 1;; *) command mv "$@";; esac; }
if oc_stdc_prepare; then exit 91; fi
''');assert q.returncode==0,q.stderr;assert lib.read_bytes()==old and checks.read_text()==checks_orig
 reset();q=run('''cp(){ if [ "$1" = "'''+str(src)+'''" ]; then return 1; fi; command cp "$@"; }
if oc_stdc_prepare; then exit 92; fi
''');assert q.returncode==0,q.stderr;assert lib.read_bytes()==old and checks.read_text()==checks_orig
 reset();q=run('oc_stdc_prepare && ! oc_stdc_prepare && oc_stdc_restore');assert q.returncode==0,q.stderr
 # Preattest must precede transaction preparation, which precedes donor mounts.
 text=(F/'f14-runtime.sh').read_text();payload=text[text.index('check_payload() {'):text.index('sync_app_config() {')]
 assert payload.index('hash_is "$ROM_CAM"')<payload.index('oc_stdc_prepare')
 early=(F/'post-fs-data.sh').read_text();assert early.index('verify-hal-profile.sh')<early.index('overlay "$MODDIR/system/priv-app"')
 verify=(F/'verify-hal-profile.sh').read_text();assert 'exit 1' in verify[verify.index('else'):]
print('PASS stdc++ commit/restore, copy failure, checksum-rename failure after library replacement, stale transaction refusal, preattestation ordering and unknown-ROM early refusal')
