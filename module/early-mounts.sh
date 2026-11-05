#!/system/bin/sh
# Simple-boot early mount transaction (corrected; host review 20261106).
# The caller (post-fs-data.sh) must already have confirmed the boot guard,
# verified the bounded boot identity and the ROM/GPU/SDK profile, and passed
# the read-only policy read-back gate. This file labels module trees with the
# proven scheme (including the vendor64/hw vendor_hal_file exception) and
# performs ONE validated mount transaction with exact-target ownership checks.
# No sleeps, no boot waits, no setprop, no service control, no policy calls,
# no app-data writes, no retries after a failed same-boot attempt.
MODDIR=${MODDIR:-${0%/*}}
OC_SIMPLE_BOOT_ID=/proc/sys/kernel/random/boot_id
OC_SIMPLE_MOUNTINFO=${OC_SIMPLE_MOUNTINFO:-/proc/1/mountinfo}

oc_simple_log() { echo "[early-mounts] $*"; }

# Label contract: each row carries dir-label and file-label. Only the closed
# set of contexts this design actually uses is accepted — never an arbitrary
# u:object_r:* — and '-' means no relabel (matches the proven rc7 post-fs-data
# app/config mounts that ran directly after the manager restorecon pass).
oc_simple_label_one() {
	# $1 relative  $2 dir-label  $3 file-label
	if [ "$2" != - ]; then
		chcon -R "$2" "$MODDIR/$1" || return 1
	fi
	if [ "$3" != - ]; then
		find "$MODDIR/$1" -type f | while IFS= read -r f; do
			chcon "$3" "$f" || exit 1
		done || return 1
	fi
	if [ "$1" = f14_runtime/vendor64 ]; then
		# Proven exception (withdrawn f14-runtime.sh enable_profile, lines 198-202):
		# the overlay contributes the merged hw directory inode, which must keep
		# the ROM's vendor_hal_file type so software-codec clients can discover
		# passthrough graphics mappers beneath it; generic vendor_file hides them
		# and GraphicBufferMapper aborts on a fresh codec start (Twitter playback).
		chcon u:object_r:vendor_hal_file:s0 "$MODDIR/f14_runtime/vendor64/hw" || return 1
	fi
	if [ "$1" = f14_runtime/provider_hw ]; then
		# Proven exception: the two provider HAL binaries carry the camera
		# executable type (hal_camera_default -> *_exec execute_no_trans allow).
		chcon u:object_r:hal_camera_default_exec:s0 \
			"$MODDIR/$1/android.hardware.camera.provider@2.4-service_64" \
			"$MODDIR/$1/occam_f14_stack/provider.real" || return 1
	fi
	return 0
}

oc_simple_validate() {
	OC_SIMPLE_ROWS=0
	while read -r kind relative target dirlabel filelabel extra; do
		case "$kind" in ''|'#'*) continue ;; overlay|replace) ;; *) return 1 ;; esac
		# Exactly five fields; anything else is malformed and refuses the plan.
		[ -z "$extra" ] || return 1
		case "$target" in
			/system/priv-app|/system/etc/permissions|/product/etc/default-permissions|/vendor/lib64|/vendor/lib64/camera|/vendor/etc/camera|/vendor/bin/hw|/odm/etc/camera|/odm/lib/rfsa/adsp|/vendor/lib/rfsa/adsp) ;;
			*) return 1 ;;
		esac
		case "$relative" in ''|/*|*..*) return 1 ;; esac
		for lbl in "$dirlabel" "$filelabel"; do
			case "$lbl" in
				-|u:object_r:vendor_file:s0|u:object_r:same_process_hal_file:s0|u:object_r:vendor_configs_file:s0|u:object_r:hal_camera_default_exec:s0) ;;
				*) return 1 ;;
			esac
		done
		[ ! -L "$MODDIR/$relative" ] && [ -d "$MODDIR/$relative" ] && [ -d "$target" ] || return 1
		OC_SIMPLE_ROWS=$((OC_SIMPLE_ROWS + 1))
	done < "$MODDIR/early-mounts.tsv"
	[ "$OC_SIMPLE_ROWS" -gt 0 ]
}

# Ownership checks: a mount is ours only at the EXACT requested target with
# our tree as its lowerdir prefix. Substring matches elsewhere in mountinfo
# do not count. /product is a symlink to /system/product on this ROM, so the
# recorded mount point is the canonical path.
oc_simple_owns_overlay() {
	OC_TARGET=$(readlink -f "$2" 2>/dev/null) || OC_TARGET=$2
	awk -v t="$OC_TARGET" -v s="lowerdir=$MODDIR/$1:" '
		$5 == t && index($0, s) > 0 { found=1 }
		END { exit !found }' "$OC_SIMPLE_MOUNTINFO"
}

# Replace rows use an empty bottom layer so the visible directory is exactly
# the module tree. The directory must stay empty; a file here would appear
# wherever the module tree does not already have that name.
oc_simple_prepare_empty() {
	[ ! -L "$MODDIR/overlay-empty" ] || return 1
	mkdir -p "$MODDIR/overlay-empty" || return 1
	[ -z "$(ls -A "$MODDIR/overlay-empty" 2>/dev/null)" ] || return 1
}

oc_simple_mount_one() {
	# $1 kind  $2 relative  $3 target
	case "$1" in
		overlay)
			oc_simple_owns_overlay "$2" "$3" && return 0
			nsenter -t 1 -m -- /system/bin/mount -t overlay overlay \
				-o "lowerdir=$MODDIR/$2:$3" "$3" || return 1 ;;
		replace)
			oc_simple_owns_overlay "$2" "$3" && return 0
			nsenter -t 1 -m -- /system/bin/mount -t overlay overlay \
				-o "lowerdir=$MODDIR/$2:$MODDIR/overlay-empty" "$3" || return 1 ;;
	esac
	OC_SIMPLE_NEW="$3 $OC_SIMPLE_NEW"
}

oc_simple_unwind() {
	# Only mounts positively created by this invocation qualify. Foreign
	# entries — including a foreign mount at one of our targets — are never
	# unmounted here.
	for target in $OC_SIMPLE_NEW; do
		nsenter -t 1 -m -- /system/bin/umount "$target" || return 1
	done
	OC_SIMPLE_NEW=
}

oc_simple_mounts() {
	OC_SIMPLE_BOOT=$(cat "$OC_SIMPLE_BOOT_ID") || return 1
	[ -n "$OC_SIMPLE_BOOT" ] || return 1
	[ "$(cat "$MODDIR/.simple-mounted" 2>/dev/null)" != "$OC_SIMPLE_BOOT" ] || return 0
	if [ "$(cat "$MODDIR/.simple-attempt" 2>/dev/null)" = "$OC_SIMPLE_BOOT" ]; then
		oc_simple_log 'refusing retry of a failed transaction in this boot'
		return 1
	fi
	oc_simple_validate || { oc_simple_log 'plan validation failed before any mount'; return 1; }
	oc_simple_prepare_empty || { oc_simple_log 'replace layer is missing or not empty'; return 1; }
	printf '%s\n' "$OC_SIMPLE_BOOT" > "$MODDIR/.simple-attempt" || return 1
	OC_SIMPLE_NEW=
	while read -r kind relative target dirlabel filelabel extra; do
		case "$kind" in ''|'#'*) continue ;; overlay|replace) ;; *) oc_simple_unwind; return 1 ;; esac
		[ -z "$extra" ] || { oc_simple_unwind; return 1; }
		[ -d "$MODDIR/$relative" ] && [ -d "$target" ] || { oc_simple_unwind; return 1; }
		oc_simple_label_one "$relative" "$dirlabel" "$filelabel" || { oc_simple_unwind; return 1; }
		oc_simple_mount_one "$kind" "$relative" "$target" || { oc_simple_unwind; return 1; }
	done < "$MODDIR/early-mounts.tsv"
	if ! { printf '%s\n' "$OC_SIMPLE_BOOT" > "$MODDIR/.simple-mounted.next" &&
		mv "$MODDIR/.simple-mounted.next" "$MODDIR/.simple-mounted"; }; then
		oc_simple_unwind
		return 1
	fi
	oc_simple_log "transaction complete: $OC_SIMPLE_ROWS rows"
}

oc_simple_verify() {
	# Read-only: every row must be owned at its exact target. Used by the
	# health reporter only; it never repairs anything.
	oc_simple_validate || return 1
	while read -r kind relative target dirlabel filelabel extra; do
		case "$kind" in
			''|'#'*) continue ;;
			overlay|replace) oc_simple_owns_overlay "$relative" "$target" || return 1 ;;
		esac
	done < "$MODDIR/early-mounts.tsv"
}

case "${1:-mounts}" in
	mounts) oc_simple_mounts ;;
	verify) oc_simple_verify ;;
	*) echo 'Usage: early-mounts.sh mounts|verify' >&2; exit 2 ;;
esac
