#!/system/bin/sh
# Simple-boot installer (corrected; host review 20261106). Manager scope only:
# this runs once at install time on a fully booted system (the manager enforces
# boot-completed). It prepares the STAGED tree only — the installed module tree
# (/data/adb/modules/occam_f14_fresh) is never written; the manager's
# modules_update swap replaces it wholesale at the next boot, so no stale
# marker can carry over and an aborted install cannot destroy the old tree's
# recovery state, guard markers or transaction backups.
# Controlled device test; prepared from the verified host assembly.
ui_print 'OcCam F14 v0.8.0-rc1 — install prepares, one reboot mounts, open Camera.'
[ "$BOOTMODE" = true ] || abort 'Install through the root manager in booted Android; recovery installer unsupported.'
[ "$(getprop ro.product.cpu.abi)" = arm64-v8a ] || abort 'arm64-v8a required'
OC_SDK=$(getprop ro.build.version.sdk)
case "$OC_SDK" in 34|35|36|37) ;; *) abort 'Only attested Android 14–17 candidates are included';; esac
if [ "$KSU" = true ]; then
	printf 'ksu\n' > "$MODPATH/.root-backend"
elif [ -n "$MAGISK_VER_CODE" ]; then
	abort 'Magisk backend is detected but not qualified for this build; use KernelSU Next.'
else
	abort 'Unrecognized root manager'
fi

# The staged module root, defined explicitly: rom-profile.sh functions read
# $MODDIR/rom-profiles.tsv and $MODDIR/gpu-layouts.tsv, and the identity and
# manifest phases read staged payload files. MODDIR must point at staging,
# never at an old installed tree (which may not even be present).
MODDIR="$MODPATH"
export MODDIR

# Staged-only stale-state cleanup. The old installation is not touched here.
for OC_STALE in .boot-pending .boot-pending.next .boot-refused .boot-guard.log \
	.simple-attempt .simple-mounted .simple-mounted.next \
	.activation-blocked .runtime-rom-profile .stdcpp-transaction \
	.auto-approved .auto-ready .auto-pending .auto-attempt .auto-blocked .auto-last; do
	rm -rf "$MODPATH/$OC_STALE" 2>/dev/null
done
# Preserve a deliberately disabled state across the update (read-only on the
# old tree). OC_OLD_PATH exists only for host installer-body fixtures.
OC_OLD=/data/adb/modules/occam_f14_fresh
if [ "$OC_OLD" != "$MODPATH" ] && [ -d "$OC_OLD" ]; then
	if [ -f "$OC_OLD/f14-runtime.sh" ] && [ ! -f "$OC_OLD/payload.sha256" ]; then
		ui_print 'Previous rc7-class install detected; its runtime state ends with the current boot.'
	fi
	if [ -f "$OC_OLD/disable" ]; then
		touch "$MODPATH/disable"
		ui_print 'Existing disabled state preserved. Enable deliberately after reviewing recovery.'
	fi
fi

# Live attestation of the exact ROM/GPU/SDK tuple against the STAGED tables;
# never a saved identity, never an arbitrary profile.
. "$MODPATH/rom-profile.sh"
oc_profile_check || abort 'Unattested ROM/GPU/SDK profile; refusing to install.'
# This test candidate ships exactly the measured profile and its measured DSP
# rule. No other profile may prepare it (see ASSEMBLY.md for the residual
# one-boot cross-ROM policy window this does and does not close).
[ "$OC_PROFILE" = a16_cyborg_live ] || abort "This candidate is frozen to a16_cyborg_live; attested '$OC_PROFILE' is not shippable"
ui_print "Attested measured profile: $OC_PROFILE (sdk $OC_PROFILE_SDK, gpu $OC_PROFILE_GPU)"

# Per-profile payload preparation: the proven libstdc++ alias and the exact
# measured OpenCL copy. No donor ELF patching, no ROM GPU-driver replacement.
OC_ROW=$(grep "^$OC_PROFILE " "$MODPATH/rom-profiles.tsv") || abort 'Profile row vanished'
OC_WANT_STDC=$(printf '%s\n' "$OC_ROW" | awk '{print $5}')
cp /vendor/lib64/libstdc++_vendor.so "$MODPATH/f14_runtime/vendor64/libstdc++.so" || abort 'libstdc++ alias copy failed'
OC_GOT_STDC=$(sha256sum "$MODPATH/f14_runtime/vendor64/libstdc++.so")
[ "${OC_GOT_STDC%% *}" = "$OC_WANT_STDC" ] || abort 'libstdc++ alias hash mismatch'
if [ -n "$OC_GPU_OVERLAY_PATH" ]; then
	mkdir -p "$MODPATH/opencl_runtime"
	cp "$OC_GPU_OVERLAY_PATH" "$MODPATH/opencl_runtime/$OC_GPU_OVERLAY_NAME" || abort 'OpenCL copy failed'
	OC_GOT_GPU=$(sha256sum "$MODPATH/opencl_runtime/$OC_GPU_OVERLAY_NAME")
	[ "${OC_GOT_GPU%% *}" = "$OC_GPU_OVERLAY_HASH" ] || abort 'OpenCL copy hash mismatch'
else
	rm -rf "$MODPATH/opencl_runtime"
	grep -v '^overlay[[:space:]]opencl_runtime[[:space:]]' "$MODPATH/early-mounts.tsv" > "$MODPATH/early-mounts.tsv.next" &&
		mv "$MODPATH/early-mounts.tsv.next" "$MODPATH/early-mounts.tsv" || abort 'OpenCL row removal failed'
fi

# F.14 identity phase (corrected contract): the hook's
# CameraCapabilities.originalF14Profile requires exactly the F.14 donor pair.
# Hash the STAGED donor HAL files (never the currently mounted ROM files),
# refuse any mismatch, and write the app-readable identity into the staged
# priv-app tree. The file becomes visible to the app only when the priv-app
# overlay mounts, and the transaction unwinds completely on any mount
# failure — so a failed boot cannot leave a false active identity.
F14_CAM=847b1f9e19ef20e58cdf7ac42b0d3ed2c026b1e1b343be28471019c5de8e4ac0
F14_CHI=a0836c593f66580a7faef35463abf7bd5a049b0acb91b72a6ef16a6fbd397fce
OC_IDDIR="$MODPATH/system/priv-app/OppoCamera/fresh_oem_bootstrap"
mkdir -p "$OC_IDDIR"
STAGED_CAM=$(sha256sum "$MODPATH/f14_runtime/vendor64/hw/camera.qcom.so")
STAGED_CHI=$(sha256sum "$MODPATH/f14_runtime/vendor64/hw/com.qti.chi.override.so")
[ "${STAGED_CAM%% *}" = "$F14_CAM" ] && [ "${STAGED_CHI%% *}" = "$F14_CHI" ] ||
	abort 'Staged HAL pair is not the original F.14 donor; identity refused'
printf '{"schema":1,"camera_sha256":"%s","chi_sha256":"%s"}\n' "$F14_CAM" "$F14_CHI" \
	> "$OC_IDDIR/active_hal_profile.json" || abort 'Identity write failed'
chmod 0444 "$OC_IDDIR/active_hal_profile.json"
# Proven label (withdrawn verify-hal-profile.sh): the app reads this file from
# the overlaid system path, so it must carry the system_file context.
chcon u:object_r:system_file:s0 "$OC_IDDIR/active_hal_profile.json" || abort 'Identity label failed'

# Full payload integrity manifest — generated AFTER the identity file so the
# manifest accounts for it — then verified in full. This is the install-time
# whole-payload check (~418 files); the boot path re-checks only a bounded
# identity set (boot-identity.sha256 below), never the whole ~0.9GB tree.
(
	cd "$MODPATH" &&
	find system f14_runtime/vendor64 f14_runtime/camera_config f14_runtime/provider_hw \
		f14_runtime/vendor_camera_data rfsa_runtime vendor_rfsa_runtime opencl_runtime \
		-type f 2>/dev/null | LC_ALL=C sort | xargs sha256sum > payload.sha256
) || abort 'Payload manifest generation failed'
[ -s "$MODPATH/payload.sha256" ] || abort 'Payload manifest is empty'
grep -q 'fresh_oem_bootstrap/active_hal_profile.json' "$MODPATH/payload.sha256" ||
	abort 'Identity file is not covered by the payload manifest'
( cd "$MODPATH" && sha256sum -c payload.sha256 >/dev/null ) || abort 'Staged payload failed its full verification'

# Bounded boot identity manifest: exactly the artifacts whose substitution
# would change behavior or identity at boot (see ASSEMBLY.md for the scope).
(
	cd "$MODPATH" &&
	LC_ALL=C sort > boot-identity.sha256 <<'OC_BOOT_IDENTITY_EOF'
f14_runtime/provider_hw/android.hardware.camera.provider@2.4-service_64
f14_runtime/provider_hw/occam_f14_stack/provider.real
f14_runtime/vendor64/hw/camera.qcom.so
f14_runtime/vendor64/hw/com.qti.chi.override.so
rom-profiles.tsv
gpu-layouts.tsv
early-mounts.tsv
sepolicy.rule
system/priv-app/OppoCamera/OppoCamera.apk
system/priv-app/OppoCamera/fresh_oem_bootstrap/active_hal_profile.json
OC_BOOT_IDENTITY_EOF
	xargs sha256sum < boot-identity.sha256 > boot-identity.sha256.hashes &&
	mv boot-identity.sha256.hashes boot-identity.sha256
) || abort 'Boot identity manifest generation failed'

# Staged permissions: scripts, preflight helper and provider binaries.
set_perm_recursive "$MODPATH" 0 0 0755 0644
for OC_SCRIPT in "$MODPATH"/*.sh; do set_perm "$OC_SCRIPT" 0 0 0755; done
set_perm "$MODPATH/root-preflight" 0 0 0755
set_perm "$MODPATH/f14_runtime/provider_hw/android.hardware.camera.provider@2.4-service_64" 0 0 0755
set_perm "$MODPATH/f14_runtime/provider_hw/occam_f14_stack/provider.real" 0 0 0755
chmod 0444 "$OC_IDDIR/active_hal_profile.json" 2>/dev/null

# The measured policy rule is a plain module file; the manager applies it
# before module scripts on the next boot. Host tests freeze its content.
[ -f "$MODPATH/sepolicy.rule" ] || abort 'sepolicy.rule missing from the package'

ui_print 'Setup: LSPosed -> Modules -> OcCam Fresh: enable it and select OppoCamera (one-time).'
ui_print 'Then reboot once and open the Camera icon. There is no Action button in this build.'
ui_print 'After boot a status notification reports health; it never repairs anything.'
