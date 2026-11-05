#!/system/bin/sh
# User-invoked KSU/Magisk Action only. Never sourced or called by boot scripts.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
if [ "$(getprop sys.boot_completed)" != 1 ]; then
  echo 'Wait until Android has fully booted before loading the F14 camera profile.'
  exit 1
fi
[ ! -f "$MODDIR/disable" ] || { echo 'Enable this module first.'; exit 1; }
# Already active is read-only; do not overwrite the saved native ROM tuple.
if sh "$MODDIR/f14-runtime.sh" verify >/dev/null 2>&1; then
  echo 'F14 already active; native ROM profile preserved'; exit 0
fi
# Attest ROM BEFORE mounting the donor. Native ROM provider is fallback.
oc_profile_check || { echo 'Unattested ROM tuple; no activation'; exit 1; }
printf '%s %s %s %s %s %s\n' "$OC_PROFILE" "${OC_CAM%% *}" "${OC_CHI%% *}" "${OC_SHIM%% *}" "${OC_STDC%% *}" "$OC_PROFILE_SDK" > "$MODDIR/.runtime-rom-profile"
if ! sh "$MODDIR/access-runtime.sh" enable; then
  echo 'Manual camera dependency access failed; F14 was not started.'
  exit 1
fi
echo 'Loading the original F14 camera runtime after Android startup...'
echo 'Manual post-boot profile; provider/access setup requires this post-boot action.' > "$MODDIR/f14_runtime/manual_enabled"
if sh "$MODDIR/f14-runtime.sh" enable && sh "$MODDIR/f14-runtime.sh" health; then
  echo 'Original F14 camera profile loaded. Feature qualification varies by ROM; inspect the compatibility report.'
  am start -n com.oppo.camera/.Camera
else
  sh "$MODDIR/f14-runtime.sh" disable || echo 'Runtime rollback needs review' >&2
  sh "$MODDIR/access-runtime.sh" disable || echo 'Access rollback needs review' >&2
  echo 'F14 runtime did not activate; see the captured error above. No reboot requested.' 
  exit 1
fi
