#!/system/bin/sh
# Read-only post-boot health reporter (corrected; host review 20261106).
# KernelSU may otherwise run this through BusyBox standalone despite the shebang.
# It REPORTS status once per boot; it NEVER repairs: no policy change, no
# provider stop/start/restart, no force-stop, no permission grants, no app-data
# writes, no retries, and it never touches the disable file.
# Every check output is routed through a pipe into a private log (rc5 Binder-FD
# finding: binder shell commands fail when stderr is an inherited root-only
# file descriptor; they succeed through a pipe). The reporter distinguishes
# unconfirmed soft states from genuinely inactive/foreign stacks.
if [ -n "${ASH_STANDALONE:-}" ]; then
	unset ASH_STANDALONE
	exec /system/bin/sh "$0" "$@"
fi
MODDIR=${0%/*}
umask 077
OC_BOOT_ID=/proc/sys/kernel/random/boot_id
OC_PROVIDER_BIN=android.hardware.camera.provider@2.4-service_64
OC_PROVIDER_SVC=vendor.camera-provider-2-4
OC_PROVIDER_REAL=occam_f14_stack/provider.real
OC_APP_CAMERA_CHECK=${OC_APP_CAMERA_CHECK:-/data/user/0/com.oppo.camera/files/odm/etc/camera}
OC_CACHE_OWNER=occamfresh-original-f14-config-v1
# Live-stack identity constants (withdrawn f14-runtime.sh; hook0.124
# CameraCapabilities.originalF14Profile requires exactly the F.14 pair).
F14_CAM=847b1f9e19ef20e58cdf7ac42b0d3ed2c026b1e1b343be28471019c5de8e4ac0
F14_CHI=a0836c593f66580a7faef35463abf7bd5a049b0acb91b72a6ef16a6fbd397fce
ROM_CAM=660eef76135ef87d2c5c18cd96f2df98b1203c667889ba3460145d833d0c90ef
ROM_CHI=d409821294137000aab4d2748ce79d924b1a7fc5c24038f8c04572278c47740d

# Reuse the tested best-effort Shell notification. Never a prerequisite; text
# is passed on stdin, never interpolated into a command.
oc_notify() {
	case "$1" in
		active) OC_NOTICE_TITLE='F.14 camera runtime active';;
		unconfirmed) OC_NOTICE_TITLE='F.14 camera runtime status unconfirmed';;
		*) OC_NOTICE_TITLE='F.14 camera runtime NOT active';;
	esac
	printf '%s\n%s\n' "$OC_NOTICE_TITLE" "$2" |
		timeout 10 su 2000 -c 'IFS= read -r title; IFS= read -r body; exec cmd notification post -t "$title" -S bigtext occam_f14_health "$body"' >/dev/null 2>&1 ||
		echo 'Status notification unavailable; inspect health.last.log.' >&2
	return 0
}

oc_report_wait_boot() {
	OC_TRIES=0
	while [ "$(getprop sys.boot_completed)" != 1 ]; do
		sleep 2
		OC_TRIES=$((OC_TRIES + 1))
		[ "$OC_TRIES" -lt 150 ] || return 1
	done
	return 0
}

oc_cache_state() {
	# Report-only private cache validation; never writes and never traverses
	# symlinks. Return 0 = clean, 1 = explicit warning.
	if [ -L "$OC_APP_CAMERA_CHECK" ]; then
		echo 'cache: SYMLINK at the app camera config path; not traversed'
		return 1
	fi
	if [ ! -e "$OC_APP_CAMERA_CHECK" ]; then
		echo 'cache: none (the global early mount serves the config)'
		return 0
	fi
	if [ -L "$OC_APP_CAMERA_CHECK/config" ]; then
		echo 'cache: SYMLINK at the private config directory; not traversed'
		return 1
	fi
	if [ ! -d "$OC_APP_CAMERA_CHECK/config" ]; then
		echo 'cache: unowned or non-config content present; not touched'
		return 1
	fi
	if [ "$(cat "$OC_APP_CAMERA_CHECK/.ocfresh-f14-config" 2>/dev/null)" != "$OC_CACHE_OWNER" ]; then
		echo 'cache: unowned private config present; not touched'
		return 1
	fi
	if diff -rq "$MODDIR/f14_runtime/camera_config/config" \
		"$OC_APP_CAMERA_CHECK/config" >/dev/null 2>&1; then
		echo 'cache: owned and current'
		return 0
	fi
	echo 'cache: owned but differs from the mounted global tree (stale; report only)'
	return 1
}

oc_live_identity() {
	# Live HAL pair in init's mount namespace. f14 = our stack is live;
	# rom = a foreign/stock stack is live despite claimed mounts; unknown.
	OC_LIVE=$(nsenter -t 1 -m -- timeout 30 sha256sum \
		/vendor/lib64/hw/camera.qcom.so /vendor/lib64/hw/com.qti.chi.override.so 2>/dev/null) ||
		{ echo unreadable; return 0; }
	OC_LIVE_CAM=$(printf '%s\n' "$OC_LIVE" | awk '/camera\.qcom\.so/{print $1}')
	OC_LIVE_CHI=$(printf '%s\n' "$OC_LIVE" | awk '/chi\.override\.so/{print $1}')
	if [ "$OC_LIVE_CAM" = "$F14_CAM" ] && [ "$OC_LIVE_CHI" = "$F14_CHI" ]; then
		echo f14
	elif [ "$OC_LIVE_CAM" = "$ROM_CAM" ] && [ "$OC_LIVE_CHI" = "$ROM_CHI" ]; then
		echo rom
	else
		echo unknown
	fi
	return 0
}

oc_health_main() {
	oc_report_wait_boot || { echo 'Boot did not complete; no status claimed.'; return 1; }
	[ ! -f "$MODDIR/disable" ] && [ ! -f "$MODDIR/remove" ] || { echo 'Module disabled/removed; no status.'; return 1; }

	OC_GUARD_OK=0
	. "$MODDIR/boot-guard.sh"
	if oc_boot_guard_confirm; then OC_GUARD_OK=1; else echo 'Boot guard completion could not be confirmed.'; fi

	OC_POLICY_OK=0
	if sh "$MODDIR/policy-preflight.sh" measured; then OC_POLICY_OK=1; else echo 'Policy read-back failed or unverifiable.'; fi

	OC_MOUNTS_OK=0
	if OC_SIMPLE_MOUNTINFO=${OC_SIMPLE_MOUNTINFO:-/proc/1/mountinfo} sh "$MODDIR/early-mounts.sh" verify; then
		OC_MOUNTS_OK=1
	else
		echo 'Early mount exact-target ownership/completeness check failed.'
	fi

	# Read camera enumeration before snapshotting provider PID/state; lazy
	# registration during this query must not leave a stale inactive verdict.
	OC_CAMERAS_OK=0
	if nsenter -t 1 -m -- timeout 10 dumpsys -t 5 media.camera 2>/dev/null |
		grep -Fq 'Number of camera devices: 10'; then
		OC_CAMERAS_OK=1
	fi


	OC_PROVIDER_STATE=$(getprop "init.svc.$OC_PROVIDER_SVC")
	OC_PROVIDER_PID=$(pidof "$OC_PROVIDER_BIN" 2>/dev/null)
	OC_PROVIDER_EXE=
	if [ -n "$OC_PROVIDER_PID" ]; then
		OC_PROVIDER_EXE=$(readlink "/proc/$OC_PROVIDER_PID/exe" 2>/dev/null)
	fi
	OC_PROVIDER_OK=0
	if [ "$OC_PROVIDER_STATE" = running ] || [ -n "$OC_PROVIDER_PID" ]; then OC_PROVIDER_OK=1; fi
	OC_PROVIDER_IDENTITY_OK=0
	case "$OC_PROVIDER_EXE" in
		*"$OC_PROVIDER_REAL") OC_PROVIDER_IDENTITY_OK=1 ;;
	esac

	# Live stack identity, hw directory label, selector mapping, ten cameras.
	OC_IDENTITY=$(oc_live_identity)
	OC_IDENTITY_OK=0
	[ "$OC_IDENTITY" = f14 ] && OC_IDENTITY_OK=1
	OC_HW_LABEL_OK=0
	if nsenter -t 1 -m -- timeout 5 ls -Zd /vendor/lib64/hw 2>/dev/null |
		grep -Fq 'u:object_r:vendor_hal_file:s0'; then
		OC_HW_LABEL_OK=1
	fi
	OC_SELECTOR_PID=$(getprop "init.svc_debug_pid.$OC_PROVIDER_SVC")
	OC_SELECTOR_OK=0
	if [ -n "$OC_SELECTOR_PID" ] &&
		nsenter -t 1 -m -- timeout 5 grep -Fq '/occam_f14_stack/liboc_f14_selector.so' \
			"/proc/$OC_SELECTOR_PID/maps" 2>/dev/null; then
		OC_SELECTOR_OK=1
	fi

	OC_HOOK_OK=0
	OC_HOOK_DETAIL=$(sh "$MODDIR/hook-preflight.sh" 2>&1)
	OC_HOOK_RC=$?
	if [ "$OC_HOOK_RC" -eq 0 ]; then
		OC_HOOK_OK=1
		printf '%s\n' "$OC_HOOK_DETAIL"
	else
		echo 'Hook 0.124 / LSPosed scope check failed.'
		printf '%s\n' "$OC_HOOK_DETAIL"
		OC_HOOK_DETAIL=$(printf '%s\n' "$OC_HOOK_DETAIL" | head -n 1)
	fi

	OC_CACHE_OK=0
	oc_cache_state && OC_CACHE_OK=1

	# Soft, log-only label spot-checks of the staged trees; never gating.
	for labeled in f14_runtime/vendor64 f14_runtime/provider_hw rfsa_runtime; do
		ls -Zd "$MODDIR/$labeled" 2>/dev/null
	done

	OC_BOOT_MARK=$(cat "$OC_BOOT_ID" 2>/dev/null)
	# Core = the stack is genuinely ours and up: policy, mounts, provider and a
	# live F.14 HAL pair. A ROM or unknown live HAL despite owned mounts is a
	# genuinely foreign/inactive state, not a soft warning.
	OC_CORE_OK=0
	if [ "$OC_POLICY_OK" = 1 ] && [ "$OC_MOUNTS_OK" = 1 ] && [ "$OC_PROVIDER_OK" = 1 ] &&
		[ "$OC_IDENTITY_OK" = 1 ]; then
		OC_CORE_OK=1
	fi
	# Soft items: boot-guard marker confirmation, provider executable identity,
	# hook scope, hw label, selector mapping, camera enumeration, cache state.
	OC_EXTENDED_OK=0
	if [ "$OC_GUARD_OK" = 1 ] && [ "$OC_HOOK_OK" = 1 ] && [ "$OC_HW_LABEL_OK" = 1 ] &&
		[ "$OC_SELECTOR_OK" = 1 ] && [ "$OC_CAMERAS_OK" = 1 ] && [ "$OC_CACHE_OK" = 1 ] &&
		[ "$OC_PROVIDER_IDENTITY_OK" = 1 ]; then
		OC_EXTENDED_OK=1
	fi
	if [ "$OC_CORE_OK" = 1 ] && [ "$OC_EXTENDED_OK" = 1 ]; then
		OC_VERDICT=active
	elif [ "$OC_CORE_OK" = 1 ] || { [ "$OC_POLICY_OK" = 1 ] && [ "$OC_MOUNTS_OK" = 1 ] && [ "$OC_PROVIDER_OK" = 1 ] && [ "$OC_IDENTITY" = unreadable ]; }; then
		OC_VERDICT=unconfirmed
	else
		OC_VERDICT=inactive
	fi

	{
		printf 'boot=%s verdict=%s\n' "$OC_BOOT_MARK" "$OC_VERDICT"
		printf 'policy_ok=%s mounts_ok=%s provider_state=%s provider_pid=%s provider_exe=%s guard_ok=%s\n' \
			"$OC_POLICY_OK" "$OC_MOUNTS_OK" "$OC_PROVIDER_STATE" \
			"${OC_PROVIDER_PID:-none}" "${OC_PROVIDER_EXE:-none}" "$OC_GUARD_OK"
		printf 'live_identity=%s identity_ok=%s hw_label_ok=%s selector_ok=%s (pid=%s) cameras10_ok=%s hook_ok=%s cache_ok=%s\n' \
			"$OC_IDENTITY" "$OC_IDENTITY_OK" "$OC_HW_LABEL_OK" "$OC_SELECTOR_OK" \
			"${OC_SELECTOR_PID:-none}" "$OC_CAMERAS_OK" "$OC_HOOK_OK" "$OC_CACHE_OK"
		oc_cache_state
		echo 'This is a read-only report. No repair, policy change or provider restart was attempted.'
	}

	# A missing hook and a missing OppoCamera scope are different setup steps.
	# The preflight line names which one failed. This does not repair it.
	if [ "$OC_HOOK_OK" != 1 ]; then
		oc_notify unconfirmed "${OC_HOOK_DETAIL:-LSPosed step: OcCam Fresh could not be checked. See health.last.log.}"
		echo "Health report complete: verdict=$OC_VERDICT; hook setup required"
		return 0
	fi

	case "$OC_VERDICT" in
		active)
			oc_notify active 'F.14 runtime is active. Open OppoCamera normally. This is a startup check, not a guarantee of every feature.' ;;
		unconfirmed)
			oc_notify unconfirmed 'Core checks passed but a status item failed (hook scope, private cache, provider identity or camera enumeration). See health.last.log. No repair was attempted.' ;;
		*)
			oc_notify inactive 'F.14 runtime did not fully start; overlays may still be mounted, so the ROM camera is not guaranteed until you disable the module and reboot once. Check health.last.log; do not reboot repeatedly.' ;;
	esac
	echo "Health report complete: verdict=$OC_VERDICT"
	return 0
}

# Everything above runs with stdout/stderr on this pipe (rc5 Binder-FD fix);
# the subshell exit status is preserved through a side file, as in rc7.
rm -f "$MODDIR/.health.exit"
( oc_health_main
  printf '%s\n' "$?" > "$MODDIR/.health.exit"
) 2>&1 | tee "$MODDIR/health.last.log" >/dev/null
OC_HEALTH_RC=$(cat "$MODDIR/.health.exit" 2>/dev/null) || exit 1
case "$OC_HEALTH_RC" in ''|*[!0-9]*) exit 1;; esac
exit "$OC_HEALTH_RC"
