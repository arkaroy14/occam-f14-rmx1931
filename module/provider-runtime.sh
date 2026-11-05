#!/system/bin/sh
# Fresh-module-only runtime sensor bridge, pinned to the audited ROM HAL pair.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
SERVICE=vendor.camera-provider-2-4
PERMS='{ read open getattr map execute execute_no_trans }'

owns_mount() { grep -q "$MODDIR/provider_hw:" /proc/1/mountinfo; }
wait_ready() {
  COUNT=0
  while [ "$COUNT" -lt 25 ]; do
    if [ "$(getprop init.svc.$SERVICE)" = running ] &&
       dumpsys media.camera | grep -q 'Number of camera devices: 8'; then
      sleep 2
      return 0
    fi
    sleep 1
    COUNT=$((COUNT + 1))
  done
  return 1
}
stop_camera() {
  am force-stop com.oppo.camera >/dev/null 2>&1
  setprop ctl.stop "$SERVICE"
  COUNT=0
  while [ "$(getprop init.svc.$SERVICE)" != stopped ] && [ "$COUNT" -lt 15 ]; do
    sleep 1
    COUNT=$((COUNT + 1))
  done
  [ "$(getprop init.svc.$SERVICE)" = stopped ]
}
disable_bridge() {
  if owns_mount; then
    stop_camera || return 1
    nsenter -t 1 -m -- /system/bin/umount /vendor/bin/hw || return 1
  fi
  setprop ctl.start "$SERVICE"
  wait_ready
}
enable_bridge() {
  # Withdrawn. The old body pinned Cyborg HAL 660eef76… / CHI d4098212… and
  # could not represent the Nayem pre-overlay pair. f14-runtime.sh is the
  # activation path. disable_bridge remains so an already mounted bridge can
  # still be removed.
  echo 'Cyborg fallback provider bridge is not part of this profile' >&2
  return 1
}
verify_bridge() {
  owns_mount || return 1
  PID=$(getprop init.svc_debug_pid.$SERVICE)
  [ -n "$PID" ] && grep -q '/occam_runtime/liboc_sensor_compat.so' "/proc/$PID/maps"
}
case "$1" in
  enable) enable_bridge ;;
  verify) verify_bridge ;;
  restart)
    stop_camera && setprop ctl.start "$SERVICE" && wait_ready && verify_bridge
    ;;
  disable) disable_bridge ;;
  *) echo 'Usage: provider-runtime.sh enable|verify|restart|disable' >&2; exit 2 ;;
esac
