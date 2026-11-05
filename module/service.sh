#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
[ ! -f "$MODDIR/disable" ] || exit 0
. "$MODDIR/boot-guard.sh"
oc_boot_guard_confirm || exit 1
for p in CAMERA RECORD_AUDIO ACCESS_FINE_LOCATION ACCESS_COARSE_LOCATION READ_EXTERNAL_STORAGE WRITE_EXTERNAL_STORAGE READ_PHONE_STATE; do
  pm grant com.oppo.camera android.permission.$p >/dev/null 2>&1
done
# No automatic provider restart or F14 activation. Use the Action after boot.
