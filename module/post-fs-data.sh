#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
LOG=/data/adb/occam_f14_fresh_boot.log
exec >"$LOG" 2>&1
set -e
. "$MODDIR/boot-guard.sh"
oc_boot_guard_enter || exit 0
# A second invocation in the same boot must not stack another overlay.
if grep -q "lowerdir=$MODDIR/system/priv-app:" /proc/1/mountinfo; then
  echo 'Fresh payload already mounted this boot.'
  exit 0
fi
echo "OcCam F14 fresh boot $(date)"
[ -f "$MODDIR/disable" ] && exit 0
if grep -q '/oppocamera_clean/' /proc/self/mountinfo; then
  echo 'Refusing mixed old camera mounts.'
  exit 1
fi
RAW=3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5
GOT=$(sha256sum "$MODDIR/system/priv-app/OppoCamera/OppoCamera.apk")
[ "${GOT%% *}" = "$RAW" ] || { echo 'Original APK identity mismatch'; exit 1; }
sh "$MODDIR/verify-hal-profile.sh"
# Provider control and camera SELinux runtime patches are manual post-boot only.
# Keep early boot limited to original app/config mounts and identity attestation.
overlay() {
  [ -d "$1" ] && [ -d "$2" ]
  mount -t overlay overlay -o "lowerdir=$1:$2" "$2"
  echo "Mounted original fresh payload at $2"
}
overlay "$MODDIR/system/priv-app" /system/priv-app
overlay "$MODDIR/system/etc/permissions" /system/etc/permissions
overlay "$MODDIR/system/etc/default-permissions" /product/etc/default-permissions
overlay "$MODDIR/odm_etc_camera" /odm/etc/camera
# Only verified identical GPU/DSP file copies with corrected labels; HAL/provider ELF unchanged.
echo 'Fresh camera mounts ready.'
