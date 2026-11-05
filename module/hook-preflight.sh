#!/system/bin/sh
# Read-only prerequisite checks. No settings/database edits or camera startup.
MODDIR=${0%/*}
OC_HOOK=$(timeout 5 pm path --user 0 com.arkaroy.occamfresh) || OC_HOOK=
case "$OC_HOOK" in package:/*) OC_HOOK=${OC_HOOK#package:};; *)
 echo 'LSPosed step: OcCam Fresh 0.124 is not installed. Install that APK, enable it under LSPosed -> Modules, then select OppoCamera (com.oppo.camera).'; exit 1;; esac
OC_HOOK_HASH=$(sha256sum "$OC_HOOK") || exit 1
[ "${OC_HOOK_HASH%% *}" = 138ff59f8dd7a852710d2c89ff86083ae5e130bc9684dc6e7e93614f4772585e ] || {
 echo 'LSPosed step: the installed hook is not OcCam Fresh 0.124. Install the paired 0.124 APK, enable it under LSPosed -> Modules, then select OppoCamera (com.oppo.camera).'; exit 1;
}
OC_CAMERA=$(timeout 5 pm path --user 0 com.oppo.camera) || OC_CAMERA=
[ "$OC_CAMERA" = package:/system/priv-app/OppoCamera/OppoCamera.apk ] || {
 echo 'The original F14 system camera is not the active APK. Remove an incompatible camera update with maintainer guidance; do not clear photos or camera data.'; exit 1;
}
OC_CAMERA_HASH=$(sha256sum /system/priv-app/OppoCamera/OppoCamera.apk) || exit 1
[ "${OC_CAMERA_HASH%% *}" = 3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5 ] || {
 echo 'The active camera APK is not the original F14 donor. Activation refused.'; exit 1;
}
pidof lspd >/dev/null || { echo 'LSPosed step: the LSPosed framework is not running. Enable LSPosed, then enable OcCam Fresh and select OppoCamera (com.oppo.camera).'; exit 1; }
[ -x "$MODDIR/root-preflight" ] || { echo 'Module preflight helper is missing; reinstall the matching module ZIP.'; exit 1; }
timeout 5 "$MODDIR/root-preflight" lsposed "$OC_HOOK"
OC_CHECK=$?
case "$OC_CHECK" in
 0) echo 'Hook 0.124 and LSPosed OppoCamera scope verified.' ;;
 21) echo 'LSPosed step: OcCam Fresh is not registered. Enable it under LSPosed -> Modules, then select OppoCamera (com.oppo.camera).'; exit 1 ;;
 22) echo 'LSPosed step: OcCam Fresh is disabled. Enable it under LSPosed -> Modules, then select OppoCamera (com.oppo.camera).'; exit 1 ;;
 23) echo 'LSPosed step: OppoCamera scope is missing. Open LSPosed -> Modules -> OcCam Fresh and select OppoCamera (com.oppo.camera).'; exit 1 ;;
 25) echo 'LSPosed step: LSPosed points at a different OcCam Fresh APK. Re-enable the installed 0.124 APK and select OppoCamera (com.oppo.camera).'; exit 1 ;;
 *) echo 'LSPosed step: OcCam Fresh could not be checked safely. Open LSPosed -> Modules and confirm OcCam Fresh 0.124 is enabled with OppoCamera selected.'; exit 1 ;;
esac
