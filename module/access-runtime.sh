#!/system/bin/sh
# GPU/DSP access for one attested layout. No APK/ELF edits and no general
# vendor_file grant. Stock-name layouts do not mount an OpenCL overlay.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"

gpu_file_count() {
  find "$MODDIR/opencl_runtime" -maxdepth 1 -type f ! -name '*.previous' ! -name '*.next' | wc -l
}
check_payload() {
  [ ! -f "$MODDIR/disable" ] || return 1
  oc_profile_check || return 1
  got=$(sha256sum "$MODDIR/system/priv-app/OppoCamera/OppoCamera.apk") || return 1
  [ "${got%% *}" = 3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5 ] || return 1
  while read -r expected relative target; do
    [ -n "$expected" ] || continue
    case "$target" in
      /vendor/lib64/libOpenCL_adreno.so)
        [ -n "$OC_GPU_OVERLAY_PATH" ] || continue
        [ "$target" = "$OC_GPU_OVERLAY_PATH" ] || return 1
        [ "$expected" = "$OC_GPU_OVERLAY_HASH" ] || return 1
        ;;
    esac
    got=$(sha256sum "$MODDIR/$relative") || return 1
    [ "${got%% *}" = "$expected" ] || return 1
    got=$(sha256sum "$target") || return 1
    [ "${got%% *}" = "$expected" ] || return 1
  done < "$MODDIR/access_files.sha256"
  if [ -n "$OC_GPU_OVERLAY_PATH" ]; then
    [ "$(gpu_file_count)" -eq 1 ] || return 1
  else
    [ "$(gpu_file_count)" -eq 0 ] || return 1
  fi
  [ "$(ls -1 "$MODDIR/rfsa_runtime" | wc -l)" -eq 1 ] || return 1
  [ "$(ls -1 "$MODDIR/vendor_rfsa_runtime" | wc -l)" -eq 3 ] || return 1
}
owns() { grep -Fq "lowerdir=$MODDIR/$1:" /proc/1/mountinfo; }
mount_one() {
  owns "$1" && return 0
  nsenter -t 1 -m -- /system/bin/mount -t overlay overlay -o "lowerdir=$MODDIR/$1:$2" "$2" || return 1
  NEW_MOUNTS="$2 $NEW_MOUNTS"
}
restore_overlay_copy() {
  [ -n "$OC_GPU_OVERLAY_NAME" ] || return 0
  if [ -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.previous" ]; then
    mv "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.previous" "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME" || return 1
  elif [ "$OC_GPU_COPIED" = 1 ]; then
    rm -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME" || return 1
  fi
  rm -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.next"
}
fail_mounts() {
  for target in $NEW_MOUNTS; do
    nsenter -t 1 -m -- /system/bin/umount "$target" || return 1
  done
  restore_overlay_copy
}
enable_access() {
  oc_profile_check || return 1
  NEW_MOUNTS=
  OC_GPU_COPIED=
  if [ -n "$OC_GPU_OVERLAY_PATH" ]; then
    if ! owns opencl_runtime; then
      [ ! -e "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.previous" ] || return 1
      cp "$OC_GPU_OVERLAY_PATH" "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.next" || return 1
      OC_GPU_COPY=$(sha256sum "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.next") || return 1
      if [ "${OC_GPU_COPY%% *}" != "$OC_GPU_OVERLAY_HASH" ]; then
        rm -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.next"
        return 1
      fi
      if [ -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME" ]; then
        mv "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME" "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.previous" || return 1
      else
        OC_GPU_COPIED=1
      fi
      if ! mv "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.next" "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME"; then
        restore_overlay_copy
        return 1
      fi
    fi
  elif owns opencl_runtime; then
    echo 'Stock OpenCL layout refuses an existing OpenCL overlay' >&2
    return 1
  fi
  if ! check_payload; then
    echo 'GPU/DSP access refused: payload/ROM identity differs' >&2
    restore_overlay_copy
    return 1
  fi
  for relative in opencl_runtime rfsa_runtime vendor_rfsa_runtime; do
    [ -d "$MODDIR/$relative" ] || return 1
    chcon u:object_r:vendor_file:s0 "$MODDIR/$relative" || { restore_overlay_copy; return 1; }
  done
  while read -r expected relative target; do
    [ -n "$expected" ] || continue
    case "$target" in
      /vendor/lib64/libOpenCL_adreno.so) [ -n "$OC_GPU_OVERLAY_PATH" ] || continue ;;
    esac
    chown 0:0 "$MODDIR/$relative" || { restore_overlay_copy; return 1; }
    chmod 0644 "$MODDIR/$relative" || { restore_overlay_copy; return 1; }
    chcon u:object_r:same_process_hal_file:s0 "$MODDIR/$relative" || { restore_overlay_copy; return 1; }
  done < "$MODDIR/access_files.sha256"
  # These two statements were measured on the current phone. They are not
  # inferred for an unbooted ROM. They are domain-wide priv_app allows.
  if [ "$OC_PROFILE_DSP_POLICY" = measured ]; then
    oc_policy patch 'allow priv_app vendor_xdsp_device chr_file { getattr open read ioctl }' || { restore_overlay_copy; return 1; }
    oc_policy patch 'allow priv_app adsprpcd_file file { getattr open read }' || { restore_overlay_copy; return 1; }
  fi
  if [ -n "$OC_GPU_OVERLAY_PATH" ]; then
    mount_one opencl_runtime /vendor/lib64 || { fail_mounts; return 1; }
  fi
  mount_one rfsa_runtime /odm/lib/rfsa/adsp || { fail_mounts; return 1; }
  mount_one vendor_rfsa_runtime /vendor/lib/rfsa/adsp || { fail_mounts; return 1; }
  rm -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME.previous"
  echo 'Measured GPU/DSP dependencies prepared for this profile. Capture modes still require qualification.'
}
verify_access() {
  check_payload || return 1
  owns rfsa_runtime && owns vendor_rfsa_runtime || return 1
  if [ -n "$OC_GPU_OVERLAY_PATH" ]; then owns opencl_runtime; else ! owns opencl_runtime; fi
}
disable_access() {
  # Live policy is not removed here. Reboot drops ksud and magiskpolicy --live
  # additions. A same-boot disable leaves those allows loaded.
  if owns vendor_rfsa_runtime; then
    nsenter -t 1 -m -- /system/bin/umount /vendor/lib/rfsa/adsp || return 1
  fi
  if owns rfsa_runtime; then
    nsenter -t 1 -m -- /system/bin/umount /odm/lib/rfsa/adsp || return 1
  fi
  if owns opencl_runtime; then
    nsenter -t 1 -m -- /system/bin/umount /vendor/lib64 || return 1
  fi
  if [ -n "$OC_GPU_OVERLAY_NAME" ] && [ "$OC_GPU_COPIED" = 1 ]; then
    rm -f "$MODDIR/opencl_runtime/$OC_GPU_OVERLAY_NAME" || return 1
  fi
}
case "$1" in
  enable) enable_access ;;
  verify) verify_access ;;
  disable) disable_access ;;
  *) echo 'Usage: access-runtime.sh enable|verify|disable' >&2; exit 2 ;;
esac
