#!/system/bin/sh
# Simple-boot controller (corrected; host review 20261106). Module post-fs-data stage.
# Order: guard -> disable check -> bounded boot identity -> live profile ->
# policy READ-BACK gate -> one early mount transaction.
# Failed qualification of ANY kind disables future attempts and preserves
# evidence (stop criteria): it never silently cycles through the manager's
# early native rule loader on later boots. This script NEVER loads or patches
# policy, never controls services (no ctl.*), never waits on Android boot,
# and never writes app data. It starts AFTER the manager's sepolicy.rule load,
# so it cannot protect that loader in the boot where a failure happens; the
# disable file only excludes this module (and its rule) from the NEXT boot.
if [ -n "${ASH_STANDALONE:-}" ]; then
	unset ASH_STANDALONE
	exec /system/bin/sh "$0" "$@"
fi
MODDIR=${0%/*}
LOG=${OC_BOOT_LOG:-/data/adb/occam_f14_fresh_boot.log}
OC_BOOT_ID=/proc/sys/kernel/random/boot_id
exec >"$LOG" 2>&1

# Failed qualification: disable for the next boot (the manager's rule loader
# skips disabled modules), keep the refusal marker and all logs as evidence,
# and stop. Re-enabling is a deliberate user act after reviewing the evidence.
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

# Bounded boot identity (see ASSEMBLY.md): identity-critical artifacts only.
# The complete payload was hashed at installation; this bounded pass exists so
# a wrong/tampered identity-critical file cannot reach the transaction. It is
# NOT a whole-payload check, and it is not a mere installer-success flag.
if ! ( cd "$MODDIR" && timeout 30 sha256sum -c boot-identity.sha256 >/dev/null 2>&1 ); then
	oc_refuse 'bounded boot identity verify (missing, changed or timed out)'
fi
RAW=3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5
GOT=$(sha256sum "$MODDIR/system/priv-app/OppoCamera/OppoCamera.apk")
[ "${GOT%% *}" = "$RAW" ] || oc_refuse 'original APK identity'

. "$MODDIR/rom-profile.sh"
oc_profile_check || oc_refuse 'ROM/GPU/SDK profile'
[ "$OC_PROFILE" = a16_cyborg_live ] && [ "$OC_PROFILE_SDK" = 36 ] && [ "$OC_PROFILE_GPU" = cyborg_phone ] || oc_refuse 'profile outside measured simple-boot target'
if [ -z "$OC_GPU_OVERLAY_PATH" ] &&
	grep -q '^overlay[[:space:]]opencl_runtime[[:space:]]' "$MODDIR/early-mounts.tsv"; then
	oc_refuse 'OpenCL overlay row present for a stock-name GPU layout'
fi

# Required policy must already exist (applied by the manager from sepolicy.rule).
# Read-back only: a denial (31) or an unverifiable state (32) refuses here.
# No live policy repair is attempted, ever. What this can and cannot contain:
# the CURRENT boot's already-loaded grants cannot be undone from userspace;
# the disable file prevents any further boot from loading the rule at all.
sh "$MODDIR/policy-preflight.sh" "$OC_PROFILE_DSP_POLICY" || oc_refuse 'required policy read-back'

sh "$MODDIR/early-mounts.sh" mounts || oc_refuse 'early mount transaction'
echo 'Simple-boot: payload mounted; provider startup belongs to Android init.'
