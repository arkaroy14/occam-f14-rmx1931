#!/usr/bin/env python3
"""Host checks for the proposed profile and policy scripts. No device, no mounts."""
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REAL_GPU = {
    'cyborg_phone': {
        'overlay': {'/vendor/lib64/libOpenCL_adreno.so':
                    '7e4cb7f681bb08d166a68e63af528cf89b39d31654c271bd63f2122409b47c1a'},
    },
    'a14_stock': {
        'keep': {
            '/vendor/lib64/libOpenCL.so': 'f64d432f19fd3f2732cd95050b47328e0c84837b7b60376d55fd7a73a6e9dc8c',
            '/vendor/lib64/libCB.so': '2e0605904e8c18c72349c4513751e43ed5bdf8af3d8ca065485f2c3057045f91',
            '/vendor/lib64/libgsl.so': '39abd031001814cc7a3d0b2911a68684eb5dcd7298c96ebf55152d915d11ff3e',
            '/vendor/lib64/libllvm-qcom.so': 'bff016d5326a0610228da1a9627120151f0dd62d1c6d2f17fbd3350e65c0cde5',
        },
    },
    'nayem_stock': {
        'keep': {
            '/vendor/lib64/libOpenCL.so': '60c51ce8802fc4793b0dbaa491808e3ff46e8d399c010a6b3e9ba4bd0d1008f6',
            '/vendor/lib64/libCB.so': 'e3f96d2c286fff3e76fdd7d15b2168b7740a406dc8b22177dc9ddd898b09fb40',
            '/vendor/lib64/libgsl.so': '24acda32d84debd36b6164f3d2e70188a1d7ef5927054b5d6a6beb0057188919',
            '/vendor/lib64/libllvm-qcom.so': '34451443c064a29a2832a5bd54150602107c52cd32d69653d629c4987e780824',
        },
    },
}


def rows(path):
    out = []
    for line in path.read_text().splitlines():
        if line.startswith('#') or not line.strip():
            continue
        out.append(line.split())
    return out


def test_real_table():
    profiles = {row[0]: row for row in rows(HERE / 'rom-profiles.tsv')}
    assert len(profiles) == 5
    layouts = {}
    for row in rows(HERE / 'gpu-layouts.tsv'):
        layouts.setdefault(row[0], []).append(row)
    assert 'UNRESOLVED_OPENCL_LAYOUT' not in (HERE / 'gpu-layouts.tsv').read_text()
    for layout, expected in REAL_GPU.items():
        got = {}
        for _layout, action, path, digest in layouts[layout]:
            got.setdefault(action, {})[path] = digest
        assert got == expected
    a15 = layouts['a15_cyborg']
    assert [row[2] for row in a15 if row[1] == 'overlay'] == ['/vendor/lib64/libOpenCL_adreno.so']
    assert profiles['a16_host_candidate'][6] == profiles['a17_host_candidate'][6] == 'nayem_stock'
    assert profiles['a16_cyborg_live'][7:] == ['optional_lineage', 'measured']
    assert all(row[7] == 'off' and row[8] == 'unmeasured' for name, row in profiles.items() if name != 'a16_cyborg_live')
    # No profile may mix Cyborg HAL with Nayem CHI.
    for row in profiles.values():
        cyborg = row[1].startswith('660eef76')
        nayem = row[1].startswith('7cd4c086')
        assert cyborg != nayem
        assert row[2].startswith('d4098212') if cyborg else row[2].startswith('a0836c59')


def run_gpu(mod, sdk, files):
    env = os.environ.copy()
    script = f'''
MODDIR={mod}
getprop() {{ echo {sdk}; }}
. {mod / 'rom-profile.sh'}
oc_profile_check
'''
    return subprocess.run(['sh', '-c', script], env=env).returncode


def test_behavior():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        mod = root / 'module'
        vendor = root / 'vendor'
        mod.mkdir()
        shutil.copy(HERE / 'rom-profile.sh', mod / 'rom-profile.sh')
        # One synthetic profile and two layouts.
        good = {}
        for name in ('camera', 'chi', 'shim', 'stdc', 'opencl', 'cb', 'gsl', 'llvm'):
            path = vendor / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode() + b'\n')
            good[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        (mod / 'rom-profiles.tsv').write_text(
            f"synth {good['camera']} {good['chi']} {good['shim']} {good['stdc']} 34 synth_stock off unmeasured\n"
            f"bad {good['camera']} {good['chi']} {good['shim']} {good['stdc']} 34 unresolved off unmeasured\n")
        (mod / 'gpu-layouts.tsv').write_text(
            f"synth_stock keep {vendor / 'opencl'} {good['opencl']}\n"
            f"synth_stock keep {vendor / 'cb'} {good['cb']}\n"
            f"synth_stock keep {vendor / 'gsl'} {good['gsl']}\n"
            f"synth_stock keep {vendor / 'llvm'} {good['llvm']}\n"
            "unresolved keep /missing UNRESOLVED_OPENCL_LAYOUT\n")
        # The checker hashes fixed /vendor paths. Point those at the temp files.
        fixed = root / 'fixed'
        hw = fixed / 'vendor/lib64/hw'
        lib = fixed / 'vendor/lib64'
        hw.mkdir(parents=True)
        (hw / 'camera.qcom.so').write_bytes((vendor / 'camera').read_bytes())
        (hw / 'com.qti.chi.override.so').write_bytes((vendor / 'chi').read_bytes())
        (lib / 'libcamera_metadata_shim.so').write_bytes((vendor / 'shim').read_bytes())
        (lib / 'libstdc++_vendor.so').write_bytes((vendor / 'stdc').read_bytes())
        script = (HERE / 'rom-profile.sh').read_text().replace('/vendor/lib64', str(lib))
        # hw lives beside lib64, so the replacement above also rewrites hw if
        # the string is the lib64 prefix. Rewrite the two hw paths explicitly.
        script = script.replace(f'{lib}/hw/camera.qcom.so', str(hw / 'camera.qcom.so'))
        script = script.replace(f'{lib}/hw/com.qti.chi.override.so', str(hw / 'com.qti.chi.override.so'))
        (mod / 'rom-profile.sh').write_text(script)
        assert run_gpu(mod, 34, good) == 0
        # Wrong companion hash fails closed.
        (vendor / 'cb').write_bytes(b'different\n')
        assert run_gpu(mod, 34, good) != 0
        (vendor / 'cb').write_bytes(b'cb\n')
        assert run_gpu(mod, 35, good) != 0
        # The unresolved token is not accepted as a wildcard.
        (mod / 'rom-profiles.tsv').write_text(
            f"synth {good['camera']} {good['chi']} {good['shim']} {good['stdc']} 34 unresolved off unmeasured\n")
        assert run_gpu(mod, 34, good) != 0


def test_policy():
    source = (HERE / 'root-policy.sh').read_text()
    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        capture = t / 'args'
        mock = '#!/usr/bin/env python3\nimport sys,json,os\nopen(os.environ["CAPTURE"],"w").write(json.dumps(sys.argv[1:]))\n'
        ksu = t / 'ksud'
        magisk = t / 'magiskpolicy'
        for path in (ksu, magisk):
            path.write_text(mock)
            path.chmod(0o755)
        script = source.replace('/data/adb/ksud', str(ksu)).replace(
            '/data/adb/ksu/bin/ksud', str(t / 'missing-ksud')).replace(
            '/data/adb/magisk/magiskpolicy', str(magisk))
        policy = t / 'root-policy.sh'
        policy.write_text(script)
        env = {**os.environ, 'CAPTURE': str(capture)}
        both = subprocess.run(['sh', '-c', '. "$1"; oc_policy patch allow-test', 'test', str(policy)], env=env)
        assert both.returncode != 0
        ksu.unlink()
        for op, arg, expected in (
            ('patch', 'allow priv_app adsprpcd_file file { read open }', ['--live', 'allow priv_app adsprpcd_file file { read open }']),
            ('apply', str(t / 'rules'), ['--live', '--apply', str(t / 'rules')]),
        ):
            subprocess.run(['sh', '-c', '. "$1"; oc_policy "$2" "$3"', 'test', str(policy), op, arg], env=env, check=True)
            assert json.loads(capture.read_text()) == expected
        magisk.unlink()
        ksu.write_text(mock)
        ksu.chmod(0o755)
        subprocess.run(['sh', '-c', '. "$1"; oc_policy patch one-rule', 'test', str(policy)], env=env, check=True)
        assert json.loads(capture.read_text()) == ['sepolicy', 'patch', 'one-rule']
        ksu.unlink()
        missing = subprocess.run(['sh', '-c', '. "$1"; oc_policy patch one-rule', 'test', str(policy)], env=env)
        assert missing.returncode != 0


def main():
    for script in HERE.glob('*.sh'):
        subprocess.run(['sh', '-n', str(script)], check=True)
    test_real_table()
    test_behavior()
    test_policy()
    print('PASS proposed profile table, fail-closed GPU hashes, and KSU/Magisk argument selection')


if __name__ == '__main__':
    main()
