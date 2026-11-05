#!/system/bin/sh
# Manual post-boot camera performance compatibility. Never called at boot.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
PWR="$MODDIR/camera_power"
STATE="$PWR/session.env"
MARKER=/data/vendor/camera/ocfresh_camera_power
SOCKET=/data/vendor/camera/ocfresh_camera_power.sock
TARGET=/vendor/etc/powerhint.json
SERVICE=vendor.power-hal-aidl
ALLOW='allow hal_camera_default vendor_camera_data_file sock_file write'
DENY='deny hal_camera_default vendor_camera_data_file sock_file write'
ocpower_hash() { sha256sum "$1" | cut -d ' ' -f 1; }
process_matches() { [ -r "/proc/$1/cmdline" ] && tr '\000' ' ' < "/proc/$1/cmdline" | grep -Fq "$2"; }
mounted() { grep -F " $TARGET " /proc/1/mountinfo >/dev/null; }
baseline() {
 [ "$(cat /proc/sys/kernel/sched_upmigrate | tr '\t' ' ')" = '95 95' ] &&
 [ "$(cat /proc/sys/kernel/sched_downmigrate | tr '\t' ' ')" = '85 85' ]
}
reload_power() {
 setprop ctl.restart "$SERVICE"
 POWER_TRIES=0
 while [ "$POWER_TRIES" -lt 30 ]; do
  if dumpsys android.hardware.power.IPower/default 2>/dev/null | grep -Fq 'HintManager Running: true'; then
   for POWER_HINT in $POWER_HINTS; do "$PWR/vote" --restore "$POWER_HINT" || return 1; done
   return 0
  fi
  sleep 0.2; POWER_TRIES=$((POWER_TRIES+1))
 done
 return 1
}
cleanup_owned() {
 [ -f "$STATE" ] || return 0
 . "$STATE"
 # State is root-owned and contains only generated numbers/fixed hint names.
 [ "$(cat "$MARKER" 2>/dev/null)" != "$POWER_TOKEN" ] || rm -f "$MARKER"
 if process_matches "$POWER_BROKER" "$PWR/broker $POWER_PROVIDER $POWER_UID"; then
  kill "$POWER_BROKER"
 fi
 sleep 3
 sh "$PWR/restore-migration.sh"
 if mounted; then
  if [ "$(ocpower_hash "$TARGET")" != "$POWER_CONFIG" ]; then
   echo 'Power config has another owner; refusing to unmount it' >&2; return 1
  fi
  nsenter -t 1 -m -- /system/bin/umount "$TARGET" || return 1
  reload_power || return 1
 fi
 : # Live policy additions persist until reboot; do not deny shared-domain access.
 if [ -S "$SOCKET" ] && [ "$(stat -c %i "$SOCKET")" = "$POWER_SOCKET_INODE" ]; then rm -f "$SOCKET"; fi
 if [ "$POWER_GUARD" != "$$" ] && process_matches "$POWER_GUARD" "$MODDIR/camera-power-runtime.sh guard $POWER_TOKEN"; then kill "$POWER_GUARD"; fi
 rm -f "$STATE" "$PWR/ready-$POWER_TOKEN"
 baseline || { echo 'Camera power stopped; external migration settings preserved or restoration needs review' >&2; return 1; }
 echo 'Camera power stopped; normal CPU/migration settings restored'
}
cleanup() {
 [ -f "$STATE" ] || return 0
 POWER_LOCK_TRIES=0
 until mkdir "$PWR/cleanup.lock" 2>/dev/null; do
  [ -f "$STATE" ] || return 0
  POWER_LOCK_TRIES=$((POWER_LOCK_TRIES+1)); [ "$POWER_LOCK_TRIES" -lt 50 ] || return 1
  sleep 0.2
 done
 cleanup_owned; POWER_CLEANUP_RESULT=$?
 rmdir "$PWR/cleanup.lock"
 return "$POWER_CLEANUP_RESULT"
}
status() {
 [ -f "$STATE" ] || return 1
 . "$STATE"
 [ "$(cat /proc/sys/kernel/random/boot_id)" = "$POWER_BOOT" ] &&
 [ "$(cat "$MARKER" 2>/dev/null)" = "$POWER_TOKEN" ] &&
 [ "$(getprop init.svc_debug_pid.vendor.camera-provider-2-4)" = "$POWER_PROVIDER" ] &&
 [ "$(ocpower_hash "$TARGET")" = "$POWER_CONFIG" ] &&
 process_matches "$POWER_BROKER" "$PWR/broker $POWER_PROVIDER $POWER_UID" &&
 process_matches "$POWER_GUARD" "$MODDIR/camera-power-runtime.sh guard $POWER_TOKEN"
}
save_state() {
 umask 077
 printf 'POWER_TOKEN=%s\nPOWER_BOOT=%s\nPOWER_PROVIDER=%s\nPOWER_UID=%s\nPOWER_BROKER=%s\nPOWER_GUARD=%s\nPOWER_SOCKET_INODE=%s\nPOWER_CONFIG=%s\nPOWER_POLICY_ADDED=%s\nPOWER_HINTS="%s"\n' \
 "$POWER_TOKEN" "$POWER_BOOT" "$POWER_PROVIDER" "$POWER_UID" "$POWER_BROKER" "$POWER_GUARD" "$POWER_SOCKET_INODE" "$POWER_CONFIG" "$POWER_POLICY_ADDED" "$POWER_HINTS" > "$STATE.next" && mv "$STATE.next" "$STATE"
}
enable() {
 [ "$(getprop sys.boot_completed)" = 1 ] && [ "$(getenforce)" = Enforcing ] || return 1
 status && { echo 'Camera CPU/migration support already enabled'; return 0; }
 [ ! -f "$STATE" ] || cleanup || return 1
 sh "$MODDIR/f14-runtime.sh" verify || return 1
 (cd "$PWR" && sha256sum -c payload.sha256 >/dev/null) || return 1
 . "$PWR/pins.env"
 [ "$(ocpower_hash /vendor/bin/hw/occam_f14_stack/liboc_f14_selector.so)" = "$POWER_SELECTOR" ] || return 1
 [ "$(ocpower_hash "$TARGET")" = "$POWER_ORIGINAL" ] && ! mounted || return 1
 [ "$(getprop perf_profile)" = 0 ] && baseline || return 1
 [ ! -e "$MARKER" ] && [ ! -e "$SOCKET" ] || { echo 'Unowned camera power marker/socket; refusing activation' >&2; return 1; }
 POWER_HINTS=''
 POWER_TRIES=0
 while [ "$POWER_TRIES" -lt 15 ]; do
  POWER_HINTS=$(dumpsys android.hardware.power.IPower/default 2>/dev/null | awk '/Req[0-9]+:/ {print $2}' | sort -u)
  POWER_OTHER=0
  for POWER_HINT in $POWER_HINTS; do case "$POWER_HINT" in INTERACTIVE|DOUBLE_TAP_TO_WAKE) ;; *) POWER_OTHER=1 ;; esac; done
  [ "$POWER_OTHER" = 1 ] || break
  sleep 0.2; POWER_TRIES=$((POWER_TRIES+1))
 done
 [ "$POWER_OTHER" = 0 ] || { echo 'Active unrelated power votes; camera support left off' >&2; return 1; }
 POWER_HINTS=$(echo "$POWER_HINTS" | tr '\n' ' ')
 POWER_BOOT=$(cat /proc/sys/kernel/random/boot_id)
 POWER_TOKEN="$(date +%s)-$$"
 POWER_PROVIDER=$(getprop init.svc_debug_pid.vendor.camera-provider-2-4)
 POWER_UID=$(awk '/^Uid:/ {print $2}' "/proc/$POWER_PROVIDER/status")
 [ "$POWER_UID" = 1047 ] || return 1
 POWER_CONFIG=$(ocpower_hash "$PWR/powerhint.json")
 POWER_BROKER=0; POWER_GUARD=0; POWER_SOCKET_INODE=0; POWER_POLICY_ADDED=0
 "$PWR/broker" --policy-status > "$PWR/policy_status.log" 2>&1
 POWER_POLICY_RESULT=$?
 case "$POWER_POLICY_RESULT" in 0) ;; 10) POWER_POLICY_ADDED=1 ;; *) cat "$PWR/policy_status.log" >&2; return 1 ;; esac
 save_state || return 1
 # Independent guard covers setup failure, helper death and provider death.
 sh "$MODDIR/camera-power-runtime.sh" guard "$POWER_TOKEN" > "$PWR/guard.log" 2>&1 &
 POWER_GUARD=$!; save_state || return 1
 [ "$POWER_POLICY_ADDED" != 1 ] || oc_policy patch "$ALLOW" || { cleanup; return 1; }
 nsenter -t 1 -m -- /system/bin/mount --bind "$PWR/powerhint.json" "$TARGET" || { cleanup; return 1; }
 reload_power || { cleanup; return 1; }
 printf '%s\n' "$POWER_TOKEN" > "$MARKER"; chown cameraserver:camera "$MARKER"; chmod 0644 "$MARKER"
 "$PWR/broker" "$POWER_PROVIDER" "$POWER_UID" > "$PWR/broker.log" 2>&1 &
 POWER_BROKER=$!; save_state || return 1
 POWER_TRIES=0
 while [ "$POWER_TRIES" -lt 30 ]; do
  if grep -q '^READY ' "$PWR/broker.log"; then
   POWER_SOCKET_INODE=$(stat -c %i "$SOCKET"); save_state || return 1
   touch "$PWR/ready-$POWER_TOKEN"
   am force-stop com.oppo.camera >/dev/null 2>&1
   echo 'Camera CPU/migration support enabled: automatic rear60fps votes, finite2s leases'
   return 0
  fi
  process_matches "$POWER_BROKER" "$PWR/broker $POWER_PROVIDER $POWER_UID" || break
  sleep 0.2; POWER_TRIES=$((POWER_TRIES+1))
 done
 cat "$PWR/broker.log" >&2; cleanup; return 1
}
guard() {
 POWER_EXPECTED_TOKEN=$1; POWER_SETUP=0
 while [ -f "$STATE" ]; do
  . "$STATE"
  [ "$POWER_TOKEN" = "$POWER_EXPECTED_TOKEN" ] || exit 0
  POWER_SETUP=$((POWER_SETUP+1))
  if [ -f "$PWR/ready-$POWER_TOKEN" ]; then
   if [ "$(getprop init.svc_debug_pid.vendor.camera-provider-2-4)" != "$POWER_PROVIDER" ] ||
      [ "$(cat "$MARKER" 2>/dev/null)" != "$POWER_TOKEN" ] ||
      ! process_matches "$POWER_BROKER" "$PWR/broker $POWER_PROVIDER $POWER_UID"; then
    echo 'Camera power helper/provider/session ended; restoring'; cleanup; exit
   fi
  elif [ "$POWER_SETUP" -gt 45 ]; then
   echo 'Camera power setup timed out; restoring'; cleanup; exit
  fi
  sleep 1
 done
}
case "$1" in
 enable) enable ;;
 disable) cleanup ;;
 status) status && echo 'Camera CPU/migration support ready (votes only in qualified rear60fps)' ;;
 guard) guard "$2" ;;
 *) echo 'Usage: camera-power-runtime.sh enable|disable|status' >&2; exit 2 ;;
esac
