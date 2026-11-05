#!/system/bin/sh
# Generate an app-readable identity for the two explicitly measured workarounds.
# This does not change either HAL library or camera metadata.
MODDIR=${0%/*}
. "$MODDIR/root-policy.sh"
. "$MODDIR/rom-profile.sh"
PROFILE="$MODDIR/system/priv-app/OppoCamera/fresh_oem_bootstrap/active_hal_profile.json"
CAM=$(sha256sum /vendor/lib64/hw/camera.qcom.so)
CHI=$(sha256sum /vendor/lib64/hw/com.qti.chi.override.so)
CAM=${CAM%% *}
CHI=${CHI%% *}
if oc_profile_check; then
  printf '{"schema":1,"camera_sha256":"%s","chi_sha256":"%s"}\n' "$CAM" "$CHI" > "$PROFILE"
  chmod 0444 "$PROFILE"
  chcon u:object_r:system_file:s0 "$PROFILE"
  echo 'Attested exact ROM camera/shim tuple (host-qualified variants require runtime tests).'
else
  rm -f "$PROFILE"
  echo 'Unattested ROM: no payload mounts permitted.' >&2
  exit 1
fi
