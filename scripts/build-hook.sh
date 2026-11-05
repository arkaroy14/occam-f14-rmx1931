#!/bin/sh
set -eu
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to Android NDK r27d}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOST_TAG=${ANDROID_NDK_HOST_TAG:-linux-x86_64}
CXX="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin/aarch64-linux-android28-clang++"
mkdir -p "$ROOT/hook/app/src/main/jniLibs/arm64-v8a"
"$CXX" -std=c++17 -O2 -fPIC -shared -fvisibility=hidden -fno-exceptions -fno-rtti -nostdlib++ -Wl,-z,max-page-size=16384 -Wl,--build-id=sha1 "$ROOT/hook/app/src/main/cpp/native_paths.cpp" -o "$ROOT/hook/app/src/main/jniLibs/arm64-v8a/libocfresh.so" -llog -ldl
cd "$ROOT/hook"
./gradlew assembleDebug
