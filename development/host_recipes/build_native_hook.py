#!/usr/bin/env python3
"""Build only our native path adapter with the verified official NDK toolchain."""
from pathlib import Path
import hashlib
import json
import subprocess

project = Path(__file__).resolve().parents[1]
base = Path('/path/to/local-workspace/fresh_f14')
cpp = project / 'hook/app/src/main/cpp'
rows = json.loads((base / 'module_stage/occam_f14_fresh/copy_manifest.json').read_text())['copies']
names = sorted({Path(row['destination']).name for row in rows
                if ('/lib/arm64/' in row['destination'] or '/fresh_native/' in row['destination'])
                and '/fresh_native/front_night_dsp/' not in row['destination']})
(cpp / 'native_allowlist.h').write_text(
    '// Generated from verified original donor-copy manifest.\n#pragma once\n'
    '#include <initializer_list>\nstatic constexpr const char* kOriginalLibraries[] = {\n'
    + ''.join('    "' + name + '",\n' for name in names) + '};\n')
out = project / 'hook/app/src/main/jniLibs/arm64-v8a/libocfresh.so'
out.parent.mkdir(parents=True, exist_ok=True)
compiler = '/k3/tools/occamhook/android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++'
command = [compiler, '-std=c++17', '-O2', '-fPIC', '-shared', '-fvisibility=hidden',
           '-fno-exceptions', '-fno-rtti', '-nostdlib++', '-Wl,-z,max-page-size=16384',
           '-Wl,--build-id=sha1', str(cpp / 'native_paths.cpp'), '-o', str(out), '-llog', '-ldl']
subprocess.run(command, check=True)
dynamic = subprocess.check_output(['readelf', '-d', str(out)], text=True)
assert 'libc++_shared.so' not in dynamic
(base / 'evidence/native_hook_build.json').write_text(json.dumps(
    dict(command=command, sha256=hashlib.sha256(out.read_bytes()).hexdigest(), dynamic=dynamic), indent=2) + '\n')
print('Built our runtime adapter', out)
