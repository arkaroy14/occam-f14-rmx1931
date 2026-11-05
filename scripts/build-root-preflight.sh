#!/bin/sh
set -eu
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to Android NDK r27d}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
COMPILER="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_TAG:-linux-x86_64}/bin/aarch64-linux-android28-clang"
"$COMPILER" "$ROOT/native/preflight/root-preflight.c" -O2 -fPIE -pie -Wall -Wextra -Werror -ldl -Wl,-z,max-page-size=16384 -o "$ROOT/module/root-preflight"
