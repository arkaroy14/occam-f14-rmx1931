# Source build and binary scope

The source tree includes our hook app, native provider adapters, current module
scripts/profile tables and host recipes. It excludes the original OEM APK,
closed-source ELF/models/firmware, personal capture files and signing keys.

## Hook APK

Recorded toolchain: JDK17+ for AGP8.13.2, Gradle8.13, AndroidSDK36, NDKr27d.
Set ANDROID_SDK_ROOT or a private ignored hook/local.properties, install those
components, set ANDROID_NDK_HOME and run `sh scripts/build-hook.sh` on Linux.
Dependencies include compile-only XposedAPI82. The staged portable helper successfully compiled Java/C++ and assembled a
debug APK locally using the installed SDK/NDK and dependency cache. A clean
machine build remains unverified. No bit-for-bit reproducibility claim.

The supplied current APK is a development/debug build. Decide and test a
maintained release signing identity before public distribution. Keep private
keystores out of Git. A new signer can prevent update-in-place; plan migration
without clearing OppoCamera data. Do not re-sign or rebuild the OEM APK.

## Native provider adapters

Sources are under native/provider. Match the exact original donor identities
and preserve the retained build flags. Native build manifests in the internal
workspace are evidence, not portable end-user installers. Host recipe snapshots
under development/host_recipes still reference the original workspace layout;
they are labeled reference code, not a working clean-clone full-module build.

## Full root module

Publishing a reproducible full-module builder needs an owner-supplied firmware
input contract, reviewed per-file extraction manifest, dependency-origin table,
portable paths, official installer provenance, and packaging tests. Do not
reconstruct proprietary implementations from decompiles and call that source.
The current binary candidate is made from verified local donor copies plus
module scripts and profile tables. It is not recreated by GitHub's automatic
source archive. Clean-clone full-package build is a release gate still open.

Host test source, adapter code and packaging logic should be tagged with every
release. Source binaries, CI build outputs and signing secrets stay separate.
Original-code license and proprietary redistribution decision remain pending.
