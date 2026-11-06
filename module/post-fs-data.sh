#!/system/bin/sh
# OcCam F14 boot controller. Module post-fs-data stage (KernelSU Next
# on_post_data_fs module-script stage; Magisk post-fs-data script stage).
# Order: guard -> disable check -> bounded boot identity -> live profile ->
# policy READ-BACK gate -> one early mount transaction.
# Failed qualification of ANY kind disables future attempts and preserves
# evidence (stop criteria): it never silently cycles through the manager's
# early native rule loader on later boots. This script NEVER loads or patches
# policy, never controls services (no ctl.*, no setprop), never waits on
# Android boot, and never writes app data. It starts AFTER the manager's
# sepolicy.rule load (KSU Next: before module scripts in on_post_data_fs;
# Magisk: pre-init by magiskinit), so it cannot protect that loader in the
# boot where a failure happens. On KernelSU Next the disable file excludes
# this module from the NEXT rule load. On Magisk v28.1 a disable marker stops
# later module scripts only: magiskinit has already loaded the preinit
# aggregate, and no boot stage rewrites that aggregate. Magisk documents a
# ~35s cap on the WHOLE post-fs-data script stage. `timeout 30` bounds only
# `sha256sum -c boot-identity.sha256`. The live profile hashes, the policy
# read-back and the mount transaction are extra. That single timeout is not
# a proven bound for the stage.
if [ -n "${ASH_STANDALONE:-}" ]; then
	unset ASH_STANDALONE
	exec /system/bin/sh "$0" "$@"
fi
MODDIR=${0%/*}
LOG=${OC_BOOT_LOG:-/data/adb/occam_f14_fresh_boot.log}
OC_BOOT_ID=/proc/sys/kernel/random/boot_id
exec >"$LOG" 2>&1

# Failed qualification: disable future module scripts and, on KernelSU Next,
# the next rule load. On Magisk v28.1 the disable marker does not remove an
# already cached preinit aggregate; see RECOVERY.md. Keep the refusal marker
# and the log. Re-enabling is a deliberate user act after reviewing evidence.
oc_refuse() {
	touch "$MODDIR/disable" 2>/dev/null
	printf '%s refused %s\n' "$(cat "$OC_BOOT_ID" 2>/dev/null)" "$1" \
		> "$MODDIR/.boot-refused" 2>/dev/null
	echo "simple-boot refused: $1; module disabled for future boots; no mounts were made and no policy was changed."
	exit 1
}

. "$MODDIR/boot-guard.sh"
oc_boot_guard_enter || exit 0
[ ! -f "$MODDIR/disable" ] && [ ! -f "$MODDIR/remove" ] || exit 0
if grep -q '/oppocamera_clean/' /proc/self/mountinfo; then
	oc_refuse 'mixed legacy camera mounts present'
fi

# Bounded boot identity: identity-critical artifacts only. The complete
# payload was hashed at installation; this bounded pass exists so a
# wrong/tampered identity-critical file cannot reach the transaction. It is
# NOT a whole-payload check, and it is not a mere installer-success flag.
if ! ( cd "$MODDIR" && timeout 30 sha256sum -c boot-identity.sha256 >/dev/null 2>&1 ); then
	oc_refuse 'bounded boot identity verify (missing, changed or timed out)'
fi
RAW=3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5
# The bounded pass already hashed the APK. Require its manifest entry to
# carry the immutable donor digest, rather than hashing the same APK twice.
GOT=$(awk '$2 == "system/priv-app/OppoCamera/OppoCamera.apk" { print $1 }' "$MODDIR/boot-identity.sha256")
[ "$GOT" = "$RAW" ] || oc_refuse 'original APK identity'

. "$MODDIR/rom-profile.sh"
# Fresh native-ROM attestation every boot, before any early overlay: a saved
# identity is never sufficient. All five table tuples are eligible; an
# unknown or changed tuple refuses and disables future attempts.
oc_profile_check || oc_refuse 'ROM/GPU/SDK profile'
OC_BACKEND=$(cat "$MODDIR/.root-backend" 2>/dev/null || echo unknown)
echo "simple-boot profile=$OC_PROFILE sdk=$OC_PROFILE_SDK gpu=$OC_PROFILE_GPU table_note=$OC_PROFILE_DSP_POLICY manager=$OC_BACKEND"
if [ -z "$OC_GPU_OVERLAY_PATH" ] &&
	grep -q '^overlay[[:space:]]opencl_runtime[[:space:]]' "$MODDIR/early-mounts.tsv"; then
	oc_refuse 'OpenCL overlay row present for a stock-name GPU layout'
fi

# Required policy must already exist (applied by the manager from the staged
# sepolicy.rule: KSU Next per active module before module scripts; Magisk
# pre-init aggregate). Read-back only: a denial (31) or an unverifiable state
# (32) refuses here. No live policy repair is attempted, ever. What this can
# and cannot contain: the CURRENT boot's already-loaded grants cannot be
# undone from userspace; on KSU Next the disable file prevents any further
# boot from loading the rule, while Magisk caches the aggregate at install
# (util_functions copy_preinit_files) — a disable marker does NOT remove an
# already-cached aggregate for the CURRENT boot on Magisk. The gate class is
# derived from the STAGED RULE itself (what the manager was given), never
# from a per-profile table column or a saved identity.
OC_CLASS_TXT=$(sh "$MODDIR/policy-preflight.sh" classify) || oc_refuse 'staged rule class'
OC_RULE_CLASS=
OC_RULE_CHECK=
OC_RULE_MISSING=
while IFS='=' read -r OC_CK OC_CV; do
	case "$OC_CK" in
		class) OC_RULE_CLASS=$OC_CV;;
		check) OC_RULE_CHECK=$OC_CV;;
		missing) OC_RULE_MISSING=$OC_CV;;
	esac
done <<EOF
$OC_CLASS_TXT
EOF
[ -n "$OC_RULE_CHECK" ] || oc_refuse 'staged rule class'
echo "simple-boot rule_class=$OC_RULE_CLASS check=$OC_RULE_CHECK missing=$OC_RULE_MISSING"
sh "$MODDIR/policy-preflight.sh" "$OC_RULE_CHECK" || oc_refuse 'required policy read-back'

sh "$MODDIR/early-mounts.sh" mounts || oc_refuse 'early mount transaction'

# Pre-mount native-ROM attestation succeeded and the transaction completed:
# emit a narrowly parsed, root-owned, current-boot witness for the READ-ONLY
# reporter. It carries no activation authority — the next boot re-attests the
# native ROM from the real /vendor paths before any mount, exactly as here.
OC_W_TABLE=$(sha256sum "$MODDIR/rom-profiles.tsv" 2>/dev/null | awk '{print $1}')
OC_W_RULE=$(sha256sum "$MODDIR/sepolicy.rule" 2>/dev/null | awk '{print $1}')
( umask 077
  { printf 'boot=%s\n' "$(cat "$OC_BOOT_ID" 2>/dev/null)"
    printf 'profile=%s\n' "$OC_PROFILE"
    printf 'manager=%s\n' "$OC_BACKEND"
    printf 'rule_class=%s\n' "$OC_RULE_CLASS"
    printf 'check=%s\n' "$OC_RULE_CHECK"
    printf 'missing=%s\n' "$OC_RULE_MISSING"
    printf 'table=%s\n' "$OC_W_TABLE"
    printf 'rule=%s\n' "$OC_W_RULE"
  } > "$MODDIR/.boot-witness.next" && mv "$MODDIR/.boot-witness.next" "$MODDIR/.boot-witness"
) || oc_refuse 'boot witness write failed'
echo 'Simple-boot: payload mounted; provider startup belongs to Android init.'
