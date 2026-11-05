#!/system/bin/sh
# Root-manager installer scope only. No service control, policy load or mounts.
ui_print 'OcCam F14 compatibility candidate — experimental, installed DISABLED'
[ "$BOOTMODE" = true ] || abort 'Install through the root manager in booted Android; recovery installer unsupported.'
[ "$(getprop ro.product.cpu.abi)" = arm64-v8a ] || abort 'arm64-v8a required'
OC_SDK=$(getprop ro.build.version.sdk)
case "$OC_SDK" in 34|35|36|37) ;; *) abort 'Only attested Android 14–17 candidates are included';; esac
if [ "$KSU" = true ]; then
 printf 'ksu\n' > "$MODPATH/.root-backend"
elif [ -n "$MAGISK_VER_CODE" ]; then
 printf 'magisk\n' > "$MODPATH/.root-backend"
else
 abort 'Unrecognized root manager'
fi
set_perm_recursive "$MODPATH" 0 0 0755 0644
for OC_SCRIPT in "$MODPATH"/*.sh; do set_perm "$OC_SCRIPT" 0 0 0755; done
set_perm "$MODPATH/camera_power/broker" 0 0 0755
set_perm "$MODPATH/camera_power/vote" 0 0 0755
set_perm "$MODPATH/camera_power/restore-migration.sh" 0 0 0755
set_perm "$MODPATH/f14_runtime/provider_hw/android.hardware.camera.provider@2.4-service_64" 0 0 0755
set_perm "$MODPATH/f14_runtime/provider_hw/occam_f14_stack/provider.real" 0 0 0755
# Retain disabled state even if a manager normally enables a newly installed module.
touch "$MODPATH/disable"
rm -f "$MODPATH/.boot-pending" "$MODPATH/.boot-pending.next"
ui_print 'Read RECOVERY.md and QUALIFICATION.md before enabling. HAL uses manual Action only.'
