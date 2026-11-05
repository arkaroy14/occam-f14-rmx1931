#!/system/bin/sh
# Source from module runtime scripts. This does not change policy by itself.
# Live rules from either backend stay loaded until reboot. deny is not used
# here as an undo operation.
oc_policy() {
 OC_POLICY_OP=$1; shift
 OC_BACKEND=${OC_ROOT_BACKEND:-}
 if [ -z "$OC_BACKEND" ] && [ -f "$MODDIR/.root-backend" ]; then
  read -r OC_BACKEND < "$MODDIR/.root-backend" || return 1
  case "$OC_BACKEND" in ksu|magisk) ;; *) return 1;; esac
 fi
 if [ -z "$OC_BACKEND" ]; then
  OC_HAS_KSU=0
  OC_HAS_MAGISK=0
  if [ -x /data/adb/ksud ] || [ -x /data/adb/ksu/bin/ksud ]; then OC_HAS_KSU=1; fi
  if [ -x /data/adb/magisk/magiskpolicy ]; then OC_HAS_MAGISK=1; fi
  if [ "$OC_HAS_KSU" = 1 ] && [ "$OC_HAS_MAGISK" = 1 ]; then
   echo 'Both ksud and magiskpolicy exist; set OC_ROOT_BACKEND=ksu or magisk' >&2
   return 1
  fi
  if [ "$OC_HAS_KSU" = 1 ]; then OC_BACKEND=ksu
  elif [ "$OC_HAS_MAGISK" = 1 ]; then OC_BACKEND=magisk
  else
   echo 'No supported live policy backend' >&2
   return 1
  fi
 fi
 case "$OC_BACKEND" in
  ksu)
   for OC_KSUD in /data/adb/ksud /data/adb/ksu/bin/ksud; do
    if [ -x "$OC_KSUD" ]; then "$OC_KSUD" sepolicy "$OC_POLICY_OP" "$@"; return $?; fi
   done
   return 1 ;;
  magisk)
   OC_MAGISKPOLICY=/data/adb/magisk/magiskpolicy
   [ -x "$OC_MAGISKPOLICY" ] || return 1
   case "$OC_POLICY_OP" in
    apply) "$OC_MAGISKPOLICY" --live --apply "$1" ;;
    patch) "$OC_MAGISKPOLICY" --live "$1" ;;
    *) return 2 ;;
   esac ;;
  *) return 2 ;;
 esac
}
