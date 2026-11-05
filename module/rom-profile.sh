#!/system/bin/sh
# Profiles are complete tuples, never independent hash allowlists.
oc_profile_find() {
 OC_P_CAM=$1; OC_P_CHI=$2; OC_P_SHIM=$3; OC_P_STDC=$4
 while read -r OC_ID OC_CAM OC_CHI OC_SHIM OC_STDC OC_SDK OC_GPU OC_POWER OC_DSP_POLICY; do
  case "$OC_ID" in ''|'#'*) continue ;; esac
  if [ "$OC_P_CAM" = "$OC_CAM" ] && [ "$OC_P_CHI" = "$OC_CHI" ] &&
     [ "$OC_P_SHIM" = "$OC_SHIM" ] && [ "$OC_P_STDC" = "$OC_STDC" ]; then
   OC_PROFILE=$OC_ID
   OC_PROFILE_SDK=$OC_SDK
   OC_PROFILE_GPU=$OC_GPU
   OC_PROFILE_POWER=$OC_POWER
   OC_PROFILE_DSP_POLICY=$OC_DSP_POLICY
   return 0
  fi
 done < "$MODDIR/rom-profiles.tsv"
 return 1
}
oc_gpu_check() {
 OC_GPU_OVERLAY_PATH=
 OC_GPU_OVERLAY_HASH=
 OC_GPU_OVERLAY_NAME=
 OC_GPU_FOUND=
 OC_GPU_OVERLAYS=0
 while read -r OC_LAYOUT OC_GPU_ACTION OC_GPU_PATH OC_GPU_HASH; do
  case "$OC_LAYOUT" in ''|'#'*) continue ;; esac
  [ "$OC_LAYOUT" = "$OC_PROFILE_GPU" ] || continue
  OC_GPU_FOUND=1
  case "$OC_GPU_ACTION" in
   keep|overlay) ;;
   *) return 1 ;;
  esac
  # A token such as UNRESOLVED_OPENCL_LAYOUT is not a hash.
  case "$OC_GPU_HASH" in
   *[!0-9a-f]*) return 1 ;;
  esac
  [ "${#OC_GPU_HASH}" -eq 64 ] || return 1
  OC_GPU_ACTUAL=$(sha256sum "$OC_GPU_PATH") || return 1
  [ "${OC_GPU_ACTUAL%% *}" = "$OC_GPU_HASH" ] || return 1
  if [ "$OC_GPU_ACTION" = overlay ]; then
   OC_GPU_OVERLAYS=$((OC_GPU_OVERLAYS + 1))
   [ "$OC_GPU_OVERLAYS" -eq 1 ] || return 1
   OC_GPU_OVERLAY_PATH=$OC_GPU_PATH
   OC_GPU_OVERLAY_HASH=$OC_GPU_HASH
   OC_GPU_OVERLAY_NAME=${OC_GPU_PATH##*/}
  fi
 done < "$MODDIR/gpu-layouts.tsv"
 [ "$OC_GPU_FOUND" = 1 ]
}
oc_profile_check() {
 OC_CAM=$(sha256sum /vendor/lib64/hw/camera.qcom.so) || return 1
 OC_CHI=$(sha256sum /vendor/lib64/hw/com.qti.chi.override.so) || return 1
 OC_SHIM=$(sha256sum /vendor/lib64/libcamera_metadata_shim.so) || return 1
 OC_STDC=$(sha256sum /vendor/lib64/libstdc++_vendor.so) || return 1
 oc_profile_find "${OC_CAM%% *}" "${OC_CHI%% *}" "${OC_SHIM%% *}" "${OC_STDC%% *}" || return 1
 [ "$(getprop ro.build.version.sdk)" = "$OC_PROFILE_SDK" ] || return 1
 oc_gpu_check
}
