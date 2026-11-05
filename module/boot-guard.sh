#!/system/bin/sh
# Candidate only. Source in post-fs-data BEFORE ANY payload mount.
oc_boot_guard_enter() {
 [ ! -f "$MODDIR/disable" ] || return 1
 OC_BOOT=$(cat /proc/sys/kernel/random/boot_id) || return 1
 if [ -f "$MODDIR/.boot-pending" ]; then
  OC_PREVIOUS=$(cat "$MODDIR/.boot-pending") || return 1
  if [ "$OC_PREVIOUS" != "$OC_BOOT" ]; then
   printf '%s\n' 'Previous boot did not confirm completion; module disabled before payload mounts.' > "$MODDIR/boot-guard.log"
   touch "$MODDIR/disable" || return 1
   return 1
  fi
 fi
 printf '%s\n' "$OC_BOOT" > "$MODDIR/.boot-pending.next" || return 1
 # A failed rename must not look like a successful pending write.
 mv "$MODDIR/.boot-pending.next" "$MODDIR/.boot-pending" || return 1
}
oc_boot_guard_confirm() {
 [ "$(getprop sys.boot_completed)" = 1 ] || return 1
 OC_BOOT=$(cat /proc/sys/kernel/random/boot_id) || return 1
 [ "$(cat "$MODDIR/.boot-pending" 2>/dev/null)" = "$OC_BOOT" ] || return 1
 rm "$MODDIR/.boot-pending"
}
