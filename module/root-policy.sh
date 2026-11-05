#!/system/bin/sh
# Host-only candidate: no activation caller is allowed to load live policy.
oc_policy() {
 echo 'Live policy changes are disabled in this candidate. No backend was invoked.' >&2
 return 64
}
