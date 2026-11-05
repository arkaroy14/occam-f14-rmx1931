#!/system/bin/sh
# Original F14 camera stack, MANUAL POST-BOOT profile. No partition writes.
# Failed v0.5 early-boot activation is withdrawn. Never call service-control
# setprop from blocking post-fs-data. Activation requires completed Android boot.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
R="$MODDIR/f14_runtime"
SERVICE=vendor.camera-provider-2-4
PROFILE="$MODDIR/system/priv-app/OppoCamera/fresh_oem_bootstrap/active_hal_profile.json"
ROM_CAM=660eef76135ef87d2c5c18cd96f2df98b1203c667889ba3460145d833d0c90ef
ROM_CHI=d409821294137000aab4d2748ce79d924b1a7fc5c24038f8c04572278c47740d
F14_CAM=847b1f9e19ef20e58cdf7ac42b0d3ed2c026b1e1b343be28471019c5de8e4ac0
F14_CHI=a0836c593f66580a7faef35463abf7bd5a049b0acb91b72a6ef16a6fbd397fce

hash_is() { VALUE=$(sha256sum "$2") && [ "${VALUE%% *}" = "$1" ]; }
owns() {
  # F2FS bind mount roots omit /data; overlay lowerdir paths retain it.
  grep -F "${R#/data}/$1" /proc/1/mountinfo | grep -F " $2 " >/dev/null
}
any_mount() { grep -F "${R#/data}/" /proc/1/mountinfo >/dev/null; }
complete_mounts() {
  owns vendor64: /vendor/lib64 && owns vendor64/camera /vendor/lib64/camera &&
    owns camera_config /odm/etc/camera && owns provider_hw: /vendor/bin/hw &&
    { [ ! -d "$R/vendor_camera_data" ] || owns vendor_camera_data: /vendor/etc/camera; }
}
stop_provider() {
  am force-stop com.oppo.camera >/dev/null 2>&1
  setprop ctl.stop "$SERVICE"
  i=0
  while [ "$(getprop init.svc.$SERVICE)" != stopped ] && [ "$i" -lt 15 ]; do sleep 1; i=$((i+1)); done
  [ "$(getprop init.svc.$SERVICE)" = stopped ]
}
profile_identity() {
  printf '{"schema":1,"camera_sha256":"%s","chi_sha256":"%s"}\n' "$1" "$2" > "$PROFILE"
  chmod 0444 "$PROFILE"
  chcon u:object_r:system_file:s0 "$PROFILE"
}
# Transaction backups stay OUTSIDE the hashed runtime trees. Commit only
# after the separate health command passes; never silently accept stale state.
OC_TXN="$MODDIR/.stdcpp-transaction"
oc_stdc_restore() {
  [ -d "$OC_TXN" ] || return 0
  [ -f "$OC_TXN/ready" ] || { echo 'Incomplete stdc++ backup; manual review required' >&2; return 1; }
  cp "$OC_TXN/library" "$R/vendor64/libstdc++.so" &&
    cp "$OC_TXN/checks" "$R/files.sha256" || return 1
  rm -f "$R/vendor64/libstdc++.so.next" "$R/files.sha256.next"
  rm "$OC_TXN/library" "$OC_TXN/checks" "$OC_TXN/ready" && rmdir "$OC_TXN"
}
oc_stdc_prepare() {
  OC_OLD_STDC=$(sha256sum "$R/vendor64/libstdc++.so") || return 1
  OC_OLD_STDC=${OC_OLD_STDC%% *}
  [ ! -e "$OC_TXN" ] || { echo 'Unfinished stdc++ transaction; refusing retry' >&2; return 1; }
  [ "$OC_OLD_STDC" != "$OC_WANT_STDC" ] || return 0
  ! any_mount || return 1
  grep -Fx "$OC_OLD_STDC  vendor64/libstdc++.so" "$R/files.sha256" >/dev/null || return 1
  mkdir "$OC_TXN" || return 1
  cp "$R/vendor64/libstdc++.so" "$OC_TXN/library" && cp "$R/files.sha256" "$OC_TXN/checks" && touch "$OC_TXN/ready" || return 1
  cp /vendor/lib64/libstdc++_vendor.so "$R/vendor64/libstdc++.so.next" &&
    hash_is "$OC_WANT_STDC" "$R/vendor64/libstdc++.so.next" || { oc_stdc_restore; return 1; }
  sed "s/^$OC_OLD_STDC  vendor64\/libstdc++.so$/$OC_WANT_STDC  vendor64\/libstdc++.so/" "$R/files.sha256" > "$R/files.sha256.next" || { oc_stdc_restore; return 1; }
  mv "$R/vendor64/libstdc++.so.next" "$R/vendor64/libstdc++.so" &&
    mv "$R/files.sha256.next" "$R/files.sha256" || { oc_stdc_restore; return 1; }
}
oc_stdc_commit() {
  [ -d "$OC_TXN" ] || return 0
  [ -f "$OC_TXN/ready" ] || return 1
  rm "$OC_TXN/library" "$OC_TXN/checks" "$OC_TXN/ready" && rmdir "$OC_TXN"
}
check_payload() {
  [ ! -f "$MODDIR/disable" ] && [ -f "$R/manual_enabled" ] || return 1
  hash_is 3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5 "$MODDIR/system/priv-app/OppoCamera/OppoCamera.apk" || return 1
  # Record was attested by Action before donor overlays.
  [ -r "$MODDIR/.runtime-rom-profile" ] || return 1
  read -r OC_ID ROM_CAM ROM_CHI OC_WANT_SHIM OC_WANT_STDC OC_WANT_SDK < "$MODDIR/.runtime-rom-profile" || return 1
  oc_profile_find "$ROM_CAM" "$ROM_CHI" "$OC_WANT_SHIM" "$OC_WANT_STDC" || return 1
  [ "$(getprop ro.build.version.sdk)" = "$OC_WANT_SDK" ] || return 1
  hash_is "$OC_WANT_SHIM" /vendor/lib64/libcamera_metadata_shim.so || return 1
  hash_is "$OC_WANT_STDC" /vendor/lib64/libstdc++_vendor.so || return 1
  # Re-attest the GPU layout at activation. The saved line does not carry it.
  oc_gpu_check || return 1
  # Validate the native pair BEFORE any module-copy mutation.
  if ! complete_mounts; then
    hash_is "$ROM_CAM" /vendor/lib64/hw/camera.qcom.so &&
      hash_is "$ROM_CHI" /vendor/lib64/hw/com.qti.chi.override.so || return 1
  fi
  oc_stdc_prepare || return 1
  (cd "$R" && sha256sum -c files.sha256 >/dev/null) || return 1
  actual=$({ find "$R/vendor64" "$R/camera_config" "$R/provider_hw" -type f;
    [ ! -d "$R/vendor_camera_data" ] || find "$R/vendor_camera_data" -type f; } | wc -l)
  expected=$(wc -l < "$R/files.sha256")
  [ "$actual" -eq "$expected" ] || return 1
}
sync_app_config() {
  # Manual init-namespace mounts do not reach an already isolated zygote/app.
  # The original APS parser prefers this app-private path itself. Supply exact
  # pinned F14 files there; do not rewrite values or replace an unowned cache.
  OC_APP_FILES=/data/user/0/com.oppo.camera/files
  OC_APP_CAMERA="$OC_APP_FILES/odm/etc/camera"
  OC_OWNER=occamfresh-original-f14-config-v1
  [ -d "$OC_APP_FILES" ] || { echo 'F14 config cache needs one initial camera launch' >&2; return 1; }
  if [ -d "$OC_APP_CAMERA/config" ]; then
    [ "$(cat "$OC_APP_CAMERA/.ocfresh-f14-config" 2>/dev/null)" = "$OC_OWNER" ] || {
      echo 'F14 config refused: existing app-private config is not owned by this module' >&2; return 1;
    }
    if (cd "$OC_APP_CAMERA/config" && sed -n 's#  camera_config/config/#  #p' "$R/files.sha256" | sha256sum -c >/dev/null 2>&1); then return 0; fi
  fi
  OC_APP_UID=$(stat -c %u "$OC_APP_FILES") || return 1
  [ "$OC_APP_UID" -ge 10000 ] && [ "$OC_APP_UID" -lt 20000 ] || return 1
  OC_APP_CONTEXT=$(ls -Zd "$OC_APP_FILES") || return 1
  OC_APP_CONTEXT=${OC_APP_CONTEXT%% *}
  case "$OC_APP_CONTEXT" in u:object_r:privapp_data_file:s0:*) ;; *) return 1 ;; esac
  [ ! -e "$OC_APP_CAMERA/config.ocfresh-new" ] || return 1
  [ ! -e "$OC_APP_CAMERA/config.ocfresh-previous" ] || return 1
  am force-stop com.oppo.camera >/dev/null 2>&1
  mkdir -p "$OC_APP_CAMERA" || return 1
  cp -R "$R/camera_config/config" "$OC_APP_CAMERA/config.ocfresh-new" || return 1
  (cd "$OC_APP_CAMERA/config.ocfresh-new" && sed -n 's#  camera_config/config/#  #p' "$R/files.sha256" | sha256sum -c >/dev/null) || return 1
  [ ! -d "$OC_APP_CAMERA/config" ] || mv "$OC_APP_CAMERA/config" "$OC_APP_CAMERA/config.ocfresh-previous" || return 1
  mv "$OC_APP_CAMERA/config.ocfresh-new" "$OC_APP_CAMERA/config" || return 1
  printf '%s\n' "$OC_OWNER" > "$OC_APP_CAMERA/.ocfresh-f14-config" || return 1
  chown -R "$OC_APP_UID:$OC_APP_UID" "$OC_APP_FILES/odm" || return 1
  find "$OC_APP_FILES/odm" -type d -exec chmod 0700 {} \; || return 1
  find "$OC_APP_FILES/odm" -type f -exec chmod 0600 {} \; || return 1
  chcon -R "$OC_APP_CONTEXT" "$OC_APP_FILES/odm" || return 1
}
verify_active() {
  complete_mounts && hash_is "$F14_CAM" /vendor/lib64/hw/camera.qcom.so &&
    hash_is "$F14_CHI" /vendor/lib64/hw/com.qti.chi.override.so || return 1
  ls -Zd /vendor/lib64/hw | grep -F 'u:object_r:vendor_hal_file:s0' >/dev/null || return 1
  pid=$(getprop init.svc_debug_pid.$SERVICE)
  [ -n "$pid" ] && grep -q '/occam_f14_stack/liboc_f14_selector.so' "/proc/$pid/maps" || return 1
  dumpsys media.camera | grep -F 'Number of camera devices: 10' >/dev/null
}
detach_owned() {
  for target in /vendor/bin/hw /vendor/etc/camera /odm/etc/camera /vendor/lib64/camera /vendor/lib64; do
    if grep -F "${R#/data}/" /proc/1/mountinfo | grep -F " $target " >/dev/null; then
      nsenter -t 1 -m -- /system/bin/umount "$target" ||
        nsenter -t 1 -m -- /system/bin/umount -l "$target" || return 1
    fi
  done
  ! any_mount
}
disable_profile() {
  [ -r "$MODDIR/.runtime-rom-profile" ] || return 1
  read -r OC_ID ROM_CAM ROM_CHI OC_TMP1 OC_TMP2 OC_TMP3 < "$MODDIR/.runtime-rom-profile" || return 1
  oc_profile_find "$ROM_CAM" "$ROM_CHI" "$OC_TMP1" "$OC_TMP2" || return 1
  [ ! -f "$MODDIR/camera-power-runtime.sh" ] || sh "$MODDIR/camera-power-runtime.sh" disable || return 1
  stop_provider || return 1
  detach_owned || return 1
  hash_is "$ROM_CAM" /vendor/lib64/hw/camera.qcom.so &&
    hash_is "$ROM_CHI" /vendor/lib64/hw/com.qti.chi.override.so || return 1
  profile_identity "$ROM_CAM" "$ROM_CHI" || return 1
  oc_stdc_restore || return 1
  # Restart the native ROM provider; no Cyborg-specific fallback wrapper.
  setprop ctl.start "$SERVICE"
}
enable_profile() {
  check_payload || { echo 'F14 runtime refused: source/platform identity mismatch' >&2; return 1; }
  sync_app_config || return 1
  if complete_mounts; then return 0; fi
  ! any_mount || { echo 'F14 runtime has incomplete owned mounts; disable first' >&2; return 1; }
  ! grep -E '/hal_probe(_|/)' /proc/1/mountinfo >/dev/null || return 1
  hash_is "$ROM_CAM" /vendor/lib64/hw/camera.qcom.so &&
    hash_is "$ROM_CHI" /vendor/lib64/hw/com.qti.chi.override.so || return 1
  oc_policy apply "$MODDIR/manual-camera-sepolicy.rule" || return 1
  chcon -R u:object_r:vendor_file:s0 "$R/vendor64" "$R/provider_hw" || return 1
  # The overlay contributes the merged hw directory inode too. Match the
  # ROM's directory type so software-codec clients can discover passthrough
  # graphics mappers beneath it; generic vendor_file hides them from that
  # domain and causes GraphicBufferMapper to abort on a fresh codec start.
  chcon u:object_r:vendor_hal_file:s0 "$R/vendor64/hw" || return 1
  chcon -R u:object_r:vendor_configs_file:s0 "$R/camera_config" || return 1
  [ ! -d "$R/vendor_camera_data" ] || chcon -R u:object_r:vendor_configs_file:s0 "$R/vendor_camera_data" || return 1
  chcon u:object_r:hal_camera_default_exec:s0 "$R/provider_hw/android.hardware.camera.provider@2.4-service_64" || return 1
  chcon u:object_r:occam_sensor_runtime_exec:s0 "$R/provider_hw/occam_f14_stack/provider.real" || return 1
  chmod 0755 "$R/provider_hw/android.hardware.camera.provider@2.4-service_64" "$R/provider_hw/occam_f14_stack/provider.real"
  stop_provider || return 1
  if nsenter -t 1 -m -- /system/bin/mount -t overlay overlay -o "lowerdir=$R/vendor64:/vendor/lib64" /vendor/lib64 &&
     nsenter -t 1 -m -- /system/bin/mount --bind "$R/vendor64/camera" /vendor/lib64/camera &&
     nsenter -t 1 -m -- /system/bin/mount --bind "$R/camera_config" /odm/etc/camera &&
     mount_vendor_camera_data &&
     nsenter -t 1 -m -- /system/bin/mount -t overlay overlay -o "lowerdir=$R/provider_hw:/vendor/bin/hw" /vendor/bin/hw &&
     profile_identity "$F14_CAM" "$F14_CHI"; then
    setprop ctl.start "$SERVICE"
    return 0
  fi
  echo 'F14 mount setup failed; restoring fallback HAL profile' >&2
  disable_profile
  return 1
}
mount_vendor_camera_data() {
  # Only the original tonemap data is supplied here. Other ROM camera files
  # remain lower entries. Legacy F14 vendor PF_Preview_Init opens this path.
  [ ! -d "$R/vendor_camera_data" ] ||
    nsenter -t 1 -m -- /system/bin/mount -t overlay overlay -o "lowerdir=$R/vendor_camera_data:/vendor/etc/camera" /vendor/etc/camera
}
health_check() {
  i=0
  while [ "$i" -lt 35 ]; do
    verify_active && { oc_stdc_commit || return 1; echo 'Original F14 HAL profile: 10 cameras and scoped selector verified'; return 0; }
    sleep 1; i=$((i+1))
  done
  echo 'F14 runtime health failed; restoring fallback and disabling automatic retry' >&2
  [ ! -f "$R/enabled" ] || mv "$R/enabled" "$R/enabled.failed"
  [ ! -f "$R/manual_enabled" ] || mv "$R/manual_enabled" "$R/manual_enabled.failed"
  disable_profile
  return 1
}
# Read-only verify is allowed at any stage. All service-control operations must
# wait until Android boot is complete, regardless of who invokes this script.
case "$1" in
  enable|health|disable)
    [ "$(getprop sys.boot_completed)" = 1 ] || {
      echo 'F14 runtime refused: service-control operations require completed Android boot' >&2
      exit 1
    }
    ;;
esac
case "$1" in
  enable)
    if ! enable_profile; then
      oc_stdc_restore || echo "stdc++ rollback requires review" >&2
      exit 1
    fi
    # Only the measured Lineage AIDL profile may try the optional helper.
    # Any other value, including empty, leaves CPU and powerhint untouched.
    if [ "$OC_PROFILE_POWER" = optional_lineage ] && [ -f "$MODDIR/camera-power-runtime.sh" ]; then
      sh "$MODDIR/camera-power-runtime.sh" enable || echo 'Camera active; automatic performance support unavailable (see error above)' >&2
    fi
    ;;
  verify) verify_active ;;
  health) health_check ;;
  disable) rm -f "$R/enabled" "$R/manual_enabled"; disable_profile ;;
  *) echo 'Usage: f14-runtime.sh enable|verify|health|disable' >&2; exit 2 ;;
esac
