#!/system/bin/sh
# OcCam F14 installer — five attested dependency profiles, KernelSU Next and
# Magisk installation paths. Manager scope only: this runs once at install
# time on a fully booted system (the manager enforces boot-completed). It
# prepares the STAGED tree only — the installed module tree
# (/data/adb/modules/occam_f14_fresh) is never written; the manager swaps
# modules_update into place at the next boot, so an aborted install cannot
# destroy the old tree's recovery state, guard markers or transaction backups.
# Manager facts (pinned sources): KernelSU Next v3.3.0 installs to
# modules_update and swaps at handle_updated_modules before the rule load;
# Magisk stages to $SECURE_DIR/modules_update and "merges into
# $SECURE_DIR/modules in the next reboot" (details.html); both run
# customize.sh with BOOTMODE/MODPATH and provide abort/ui_print/set_perm.
# MODDIR is NOT provided by Magisk in customize.sh, hence the explicit
# definition below. Device qualification remains per-manager/per-profile:
# only the measured Cyborg A16/KernelSU environment has booted.
ui_print 'OcCam F14 — install prepares, one reboot mounts, open Camera.'
[ "$BOOTMODE" = true ] || abort 'Install through the root manager in booted Android; recovery installer unsupported.'
[ "$(getprop ro.product.cpu.abi)" = arm64-v8a ] || abort 'arm64-v8a required'
OC_SDK=$(getprop ro.build.version.sdk)
case "$OC_SDK" in 34|35|36|37) ;; *) abort 'Only attested Android 14–17 candidates are included';; esac
OC_BACKEND=unknown
if [ "$KSU" = true ]; then
	OC_BACKEND=ksu
elif [ -n "$MAGISK_VER_CODE" ]; then
	OC_BACKEND=magisk
	# Proven prerequisite (Magisk v28.1 util_functions.sh 570-586, pinned in the
	# Codex review dir): rules are aggregated into $MAGISKTMP/.magisk/preinit/
	# sepolicy.rule at install, and the caller does NOT check copy_preinit_files'
	# return — a missing preinit dir means the staged rule silently never loads.
	# Fail here with the named prerequisite instead of shipping a rule that
	# cannot be applied. MAGISKTMP unset (non-standard wrapper) also refuses.
	if [ -z "$MAGISKTMP" ] || [ ! -d "$MAGISKTMP/.magisk/preinit" ] || [ ! -w "$MAGISKTMP/.magisk/preinit" ]; then
		abort 'Magisk preinit rule cache unavailable; sepolicy.rule cannot be applied on this setup'
	fi
else
	abort 'Unrecognized root manager'
fi
printf '%s\n' "$OC_BACKEND" > "$MODPATH/.root-backend"
# The helper is invoked below during installation; manager defaults may leave
# ordinary files 0644 before customize runs, so make it executable now.
chmod 0755 "$MODPATH/root-preflight" || abort 'root-preflight chmod failed'
[ -x "$MODPATH/root-preflight" ] || abort 'root-preflight is not executable'

# The staged module root, defined explicitly: rom-profile.sh functions read
# $MODDIR/rom-profiles.tsv and $MODDIR/gpu-layouts.tsv, and the identity and
# manifest phases read staged payload files. MODDIR must point at staging,
# never at an old installed tree (which may not even be present); Magisk does
# not provide MODDIR to customize.sh at all.
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
OC_OLD=${OC_OLD_PATH:-/data/adb/modules/occam_f14_fresh}
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
# never a saved identity, never an arbitrary profile. All five table rows are
# eligible; an unknown tuple still refuses here.
. "$MODPATH/rom-profile.sh"
oc_profile_check || abort 'Unattested ROM/GPU/SDK profile; refusing to install.'
ui_print "Attested profile: $OC_PROFILE (sdk $OC_PROFILE_SDK, gpu $OC_PROFILE_GPU)"
ui_print "Root manager: $OC_BACKEND"
ui_print 'Policy capability is checked live at every install; the boot read-back'
ui_print 'verifies the actual grants again after the manager applies the rule.'

# Live policy capability (read-only). The helper scans provider-exec and both
# DSP groups. A denial (31) means that query completed. Exit 0 stages the
# packaged three-allow rule. Exit 33 means the probe ran and exec completed,
# but at least one DSP group is unverifiable: stage only the allow lines whose
# groups completed, and name the missing group. Exit 32 (or any other code)
# means the probe failed or the provider-exec context itself is unverifiable:
# refuse. Do not treat a denial, a missing API, or a non-enforcing system as
# "this ROM cannot do DSP". No per-profile class selects the rule.
# The manager applies the staged file before module scripts (Magisk: pre-init
# aggregate; KSU Next: on_post_data_fs before module scripts).
OC_LINE_EXEC=$(grep '^allow hal_camera_default hal_camera_default_exec file execute_no_trans$' "$MODPATH/sepolicy.rule")
OC_LINE_XDSP=$(grep '^allow priv_app vendor_xdsp_device chr_file { getattr open read ioctl }$' "$MODPATH/sepolicy.rule")
OC_LINE_ADS=$(grep '^allow priv_app adsprpcd_file file { getattr open read }$' "$MODPATH/sepolicy.rule")
[ -n "$OC_LINE_EXEC" ] && [ -n "$OC_LINE_XDSP" ] && [ -n "$OC_LINE_ADS" ] ||
	abort 'Packaged rule is not the measured three-allow set'
oc_write_rule() {
	{
		echo '# Staged from the packaged measured allows by policy-capability. Existing types only.'
		for OC_ALLOW in "$@"; do
			printf '%s\n' "$OC_ALLOW"
		done
	} > "$MODPATH/sepolicy.rule.staged" &&
		mv "$MODPATH/sepolicy.rule.staged" "$MODPATH/sepolicy.rule"
}
OC_CAP_RC=0
OC_CAP_OUT=$("$MODPATH/root-preflight" policy-capability 2>"$MODPATH/.policy-capability.err") || OC_CAP_RC=$?
if [ -s "$MODPATH/.policy-capability.err" ]; then
	while IFS= read -r OC_CAP_LINE; do
		ui_print "$OC_CAP_LINE"
	done < "$MODPATH/.policy-capability.err"
fi
rm -f "$MODPATH/.policy-capability.err"
case "$OC_CAP_RC" in
	0)
		ui_print 'Policy capability: live policy can express the DSP grants; staging the full measured rule.';;
	33)
		OC_EXEC_G=ok
		OC_XDSP_G=bad
		OC_ADS_G=bad
		while IFS='=' read -r OC_CK OC_CV; do
			case "$OC_CK" in
				exec) OC_EXEC_G=$OC_CV;;
				xdsp) OC_XDSP_G=$OC_CV;;
				adsprpcd) OC_ADS_G=$OC_CV;;
			esac
		done <<EOF
$OC_CAP_OUT
EOF
		[ "$OC_EXEC_G" = ok ] || abort 'Provider exec context is unverifiable; refusing fail-closed'
		if [ "$OC_XDSP_G" = ok ] && [ "$OC_ADS_G" = ok ]; then
			ui_print 'Policy capability: live policy can express the DSP grants; staging the full measured rule.'
		elif [ "$OC_XDSP_G" = ok ]; then
			oc_write_rule "$OC_LINE_EXEC" "$OC_LINE_XDSP" || abort 'Policy rule staging failed'
			ui_print 'Runtime prerequisite NOT met: adsprpcd queries are unverifiable on this live policy.'
			ui_print 'Staging provider self-exec plus vendor_xdsp only. adsprpcd capture is unqualified.'
		elif [ "$OC_ADS_G" = ok ]; then
			oc_write_rule "$OC_LINE_EXEC" "$OC_LINE_ADS" || abort 'Policy rule staging failed'
			ui_print 'Runtime prerequisite NOT met: vendor_xdsp queries are unverifiable on this live policy.'
			ui_print 'Staging provider self-exec plus adsprpcd only. vendor_xdsp capture is unqualified.'
		else
			oc_write_rule "$OC_LINE_EXEC" || abort 'Policy rule staging failed'
			ui_print 'Runtime prerequisite unverified: the full DSP policy queries did not complete.'
			ui_print 'Staging the core self-exec rule only; DSP-dependent capture remains unqualified.'
		fi
		;;
	*)
		abort "Policy capability could not be verified (helper exit $OC_CAP_RC); refusing fail-closed";;
esac
[ -f "$MODPATH/sepolicy.rule" ] || abort 'Policy rule staging failed'

# Per-profile payload preparation: the proven libstdc++ alias (each profile's
# own ROM library, never the measured phone's across profiles) and the OpenCL
# copy only for an attested overlay GPU layout. Stock-name layouts remove the
# overlay plan row so manifests and the boot transaction work without an
# opencl_runtime directory. No donor ELF patching, no ROM GPU-driver replacement.
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

# F.14 identity phase: the hook's CameraCapabilities.originalF14Profile
# requires exactly the F.14 donor pair. Hash the STAGED donor HAL files (never
# the currently mounted ROM files), refuse any mismatch, and write the
# app-readable identity into the staged priv-app tree. The file becomes
# visible to the app only when the priv-app overlay mounts, and the
# transaction unwinds completely on any mount failure — so a failed boot
# cannot leave a false active identity. Native-ROM hashes are never
# substituted for this identity.
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
# would change behavior or identity at boot. The staged (profile-selected)
# sepolicy.rule is what the manager will load, so it is the file hashed here.
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

# The staged policy rule is a plain module file; the manager applies it before
# module scripts on the next boot. Host tests freeze the content per class.
[ -f "$MODPATH/sepolicy.rule" ] || abort 'sepolicy.rule missing from the package'

ui_print 'Setup: LSPosed -> Modules -> OcCam Fresh: enable it and select OppoCamera (one-time).'
ui_print 'Then reboot once and open the Camera icon. There is no Action button in this build.'
ui_print 'After boot a status notification reports health; it never repairs anything.'
ui_print 'Device qualification status: only the measured Cyborg A16 / KernelSU Next'
ui_print 'environment has completed boot and camera verification; other profiles and'
ui_print 'Magisk are install-eligible but still need a controlled device qualification.'
