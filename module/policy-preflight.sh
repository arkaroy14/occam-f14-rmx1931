#!/system/bin/sh
# Read-only prerequisite: this script must never load/patch SELinux policy.
# `classify` names the exact allow set in the staged sepolicy.rule.
# Any other argument is the read-back scope for that set:
#   full            provider self-exec + both DSP allows (8 atomic checks)
#   core-xdsp       provider self-exec + vendor_xdsp allows only
#   core-adsprpcd   provider self-exec + adsprpcd allows only
#   core            provider self-exec only
# A core or partial set is a named missing-grant mode. It is not full-camera
# eligibility. The scope comes from the staged rule, never from a profile row.
MODDIR=${0%/*}
RULE=$MODDIR/sepolicy.rule
if [ "$1" = classify ]; then
	# Exact packaged allow sets only: unknown policy directives must not be
	# ignored merely because the three expected allow lines are also present.
	awk '
		/^[[:space:]]*(#|$)/ { next }
		$0 == "allow hal_camera_default hal_camera_default_exec file execute_no_trans" { next }
		$0 == "allow priv_app vendor_xdsp_device chr_file { getattr open read ioctl }" { next }
		$0 == "allow priv_app adsprpcd_file file { getattr open read }" { next }
		{ bad=1 }
		END { exit bad }
	' "$RULE" || { echo 'Unknown staged policy directive; refusing classification.' >&2; exit 32; }
	OC_EXEC_N=$(grep -c '^allow hal_camera_default hal_camera_default_exec file execute_no_trans$' "$RULE" 2>/dev/null || true)
	OC_XDSP_N=$(grep -c '^allow priv_app vendor_xdsp_device chr_file { getattr open read ioctl }$' "$RULE" 2>/dev/null || true)
	OC_ADS_N=$(grep -c '^allow priv_app adsprpcd_file file { getattr open read }$' "$RULE" 2>/dev/null || true)
	OC_ALLOW_N=$(grep -c '^allow ' "$RULE" 2>/dev/null || true)
	if [ "$OC_EXEC_N" = 1 ] && [ "$OC_XDSP_N" = 1 ] && [ "$OC_ADS_N" = 1 ] && [ "$OC_ALLOW_N" = 3 ]; then
		printf 'class=full\ncheck=full\nmissing=none\n'
		exit 0
	fi
	if [ "$OC_EXEC_N" = 1 ] && [ "$OC_XDSP_N" = 1 ] && [ "$OC_ADS_N" = 0 ] && [ "$OC_ALLOW_N" = 2 ]; then
		printf 'class=partial\ncheck=core-xdsp\nmissing=adsprpcd\n'
		exit 0
	fi
	if [ "$OC_EXEC_N" = 1 ] && [ "$OC_XDSP_N" = 0 ] && [ "$OC_ADS_N" = 1 ] && [ "$OC_ALLOW_N" = 2 ]; then
		printf 'class=partial\ncheck=core-adsprpcd\nmissing=xdsp\n'
		exit 0
	fi
	if [ "$OC_EXEC_N" = 1 ] && [ "$OC_XDSP_N" = 0 ] && [ "$OC_ADS_N" = 0 ] && [ "$OC_ALLOW_N" = 1 ]; then
		printf 'class=core\ncheck=core\nmissing=dsp\n'
		exit 0
	fi
	echo 'Staged rule is not a known exact allow set.' >&2
	exit 32
fi
case "$1" in
	full) OC_POLICY_SCOPE=core-dsp;;
	core) OC_POLICY_SCOPE=core;;
	core-xdsp) OC_POLICY_SCOPE=core-xdsp;;
	core-adsprpcd) OC_POLICY_SCOPE=core-adsprpcd;;
	*) echo 'Unknown policy scope; activation refused.' >&2; exit 32;;
esac
"$MODDIR/root-preflight" policy-check "$OC_POLICY_SCOPE"
OC_POLICY_RC=$?
case "$OC_POLICY_RC" in
	0) exit 0;;
	31) echo 'Required camera permissions are absent. Activation stopped; no live policy repair was attempted.' >&2;;
	*) echo 'Camera policy prerequisites could not be verified. Activation stopped without policy changes.' >&2;;
esac
exit "$OC_POLICY_RC"
