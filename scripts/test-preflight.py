from pathlib import Path
import tempfile,subprocess,sqlite3,os,hashlib
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as raw:
 t=Path(raw);db=t/'scope.db';source=t/'reader.c';binary=t/'reader'
 src=(R/'native/preflight/root-preflight.c').read_text().replace('/system/lib64/libsqlite.so','libsqlite3.so.0').replace('/data/adb/lspd/config/modules_config.db',str(db)).replace('if(geteuid()!=0)return 2;','')
 source.write_text(src);subprocess.run(['cc',str(source),'-O2','-ldl','-o',str(binary)],check=True)
 c=sqlite3.connect(db);c.executescript('CREATE TABLE modules(mid INTEGER,module_pkg_name TEXT,apk_path TEXT,enabled INTEGER); CREATE TABLE scope(mid INTEGER,app_pkg_name TEXT,user_id INTEGER);');c.commit();c.close()
 def query():return subprocess.run([str(binary),'lsposed','/test/base.apk']).returncode
 assert query()==21
 c=sqlite3.connect(db);c.execute('INSERT INTO modules VALUES(1,?,?,0)',('com.arkaroy.occamfresh','/test/base.apk'));c.commit();c.close();assert query()==22
 c=sqlite3.connect(db);c.execute('UPDATE modules SET enabled=1');c.commit();c.close();assert query()==23
 c=sqlite3.connect(db);c.execute('INSERT INTO scope VALUES(1,?,0)',('com.arkaroy.occamfresh',));c.commit();c.close();assert query()==23
 c=sqlite3.connect(db);c.execute('INSERT INTO scope VALUES(1,?,10)',('com.oppo.camera',));c.commit();c.close();assert query()==23
 c=sqlite3.connect(db);c.execute('INSERT INTO scope VALUES(1,?,0)',('com.oppo.camera',));c.commit();c.close()
 before=hashlib.sha256(db.read_bytes()).hexdigest();assert query()==0;assert hashlib.sha256(db.read_bytes()).hexdigest()==before
 c=sqlite3.connect(db);c.execute('UPDATE modules SET apk_path=?',('/stale/base.apk',));c.commit();c.close();assert query()==25
 db.unlink();assert query()==24 and not db.exists()
 print('PASS actual read-only C query: absent/disabled/self-scope/other-user/stale APK/correct scope; database unchanged and never created')
 m=t/'module';m.mkdir();(m/'hook-preflight.sh').write_bytes((R/'module/hook-preflight.sh').read_bytes());(m/'root-preflight').write_text('#!/bin/sh\nexit "${SCOPE_RC:-0}"\n');(m/'root-preflight').chmod(0o755)
 b=t/'bin';b.mkdir()
 scripts={'pm':'case "$*" in *com.oppo.camera) echo "${CAMERA_PATH:-package:/system/priv-app/OppoCamera/OppoCamera.apk}";; *) [ "${MISSING:-0}" = 1 ] || echo package:/test/base.apk;; esac','pidof':'[ "${LSPD_OFF:-0}" != 1 ]','sha256sum':'case "$1" in /system/priv-app/*) echo "${CAMERA_HASH:-3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5}  $1";; *) echo "${TEST_HASH:-138ff59f8dd7a852710d2c89ff86083ae5e130bc9684dc6e7e93614f4772585e}  /test/base.apk";; esac','timeout':'shift; exec "$@"'}
 for n,s in scripts.items():(b/n).write_text('#!/bin/sh\n'+s+'\n');(b/n).chmod(0o755)
 def check(**values):return subprocess.run(['sh',str(m/'hook-preflight.sh')],env={**os.environ,'PATH':str(b)+':'+os.environ['PATH'],**values},capture_output=True,text=True)
 assert check().returncode==0
 for e in [{'MISSING':'1'},{'LSPD_OFF':'1'},{'TEST_HASH':'wrong'},{'CAMERA_PATH':'package:/data/app/update/base.apk'},{'CAMERA_HASH':'wrong'}]+[{'SCOPE_RC':str(x)} for x in [21,22,23,24,25,124]]:
  assert check(**e).returncode!=0,e
 print('PASS script refuses missing/wrong APK, stopped framework, disabled/missing scope, unsupported DB and timeout')
