#!/system/bin/sh
# Read-only prerequisite: this script must never load/patch SELinux policy.
MODDIR=${0%/*}
case "$1" in
 measured) OC_POLICY_SCOPE=core-dsp;;
 unmeasured) OC_POLICY_SCOPE=core;;
 *) echo 'Unknown policy profile; activation refused.' >&2; exit 32;;
esac
"$MODDIR/root-preflight" policy-check "$OC_POLICY_SCOPE"
OC_POLICY_RC=$?
case "$OC_POLICY_RC" in
 0) exit 0;;
 31) echo 'Required camera permissions are absent. Activation stopped; no live policy repair was attempted.' >&2;;
 *) echo 'Camera policy prerequisites could not be verified. Activation stopped without policy changes.' >&2;;
esac
exit "$OC_POLICY_RC"
