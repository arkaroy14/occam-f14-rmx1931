# v0.8.0-rc1 — 2026-11-06 (experimental pre-release)

- Replace withdrawn Action/late activation with install-time preparation and early module mounts. No Action enrollment or per-boot manual activation.
- Preserve camera models in app namespaces with replacement overlays. Correct canonical mount-path checks and specific missing-hook/scope notifications.
- Pair hook0.124: full 4:3 front Portrait output through the measured existing reader/selector path; no temporary test flag.
- Restrict this lifecycle to measured Cyborg A16/KernelSU. Other profiles are reference-only; Magisk refused.
- Frame UI expansion deferred; original behavior unverified.
- See RELEASE_QUALIFICATION and FEATURE_STATUS for retained tests, pending final-package qualification and quality limits.

## Historical development notes

Older rc2–rc7 candidates below are withdrawn and their installation flow is superseded.

# Changelog

## v0.7.0-rc7 — final startup status candidate

- rc6 automatic boot activation and Photo/viewer passed without Action, but the notification's separate silent verify reported inactive.
- Run final read-only verification in init's mount namespace, log its return/namespace, and distinguish a successful startup with an unconfirmed final check from an inactive runtime.
- No activation retry, mount/policy change or hook/OEM-byte change. The exact cause of rc6's final-check failure was not logged; namespace mismatch remains a hypothesis.
- Same-boot status-only trial returned verified active and replaced the false warning. Fresh-boot rc7 status qualification pending.

## v0.7.0-rc6 — Binder-safe service logging candidate

- Reproduce and fix the rc5 startup failure: Android package queries rejected the inherited private log descriptor. Route stdout/stderr through a pipe; only tee opens the log. No SELinux policy widening.
- Preserve service exit status, readiness logging and once-per-boot activation protections.
- Remove the fixed 15-second startup delay. Activation begins once owner CE storage and package/profile checks are ready.
- Corrected service path activated successfully in the same boot; 10-camera health, active notification and user photo/viewer passed. rc6 new-boot qualification remains pending.
- No OEM camera bytes or hook changes. Earlier first-capture transient save report remains open.

## v0.7.0-rc5 — first-unlock startup candidate

- Wait for owner CE storage unlock before permission/package checks and automatic activation; log the waiting state immediately after completed boot.
- Normalize background service and auto entry to Android sh, retaining the manual Action shell guard.
- Log build/package preflight failures and retry only these read-only deferrals, at most six times. Preserve one actual activation per boot and interrupted/failure blocking.
- Add best-effort active/inactive startup notifications via Android Shell; manual activation replaces an inactive warning. No hook-app root grant or camera auto-launch.
- rc4 approved startup succeeded when invoked after unlock on the test device. rc5 cold-boot startup awaits testing; first-install transient black thumbnail/missing saves remain unexplained and open.
- Hook 0.113 and all OEM APK/ELF/tuning files are unchanged. Magisk/other ROM automatic boot behavior remains unqualified.

## v0.7.0-rc4 — shell-guard candidate

- Clear standalone BusyBox mode by re-entering Android sh at Action entry before locks/logging/helpers. Keep root-manager backend selection intact.
- Guard diagnostic and actual KernelSU-button activation passed, followed by user photo/viewer pass; boot ID unchanged. Fresh/automatic boot still needs testing, so no sole-root-cause claim.
- Keep manual Action on screen with a boxed test/second-tap confirmation banner for eight seconds. Do not auto-launch Camera; confirmation requires OppoCamera to be running.
- Keep all rc3 prerequisite/directory/default-enabled/qualified-auto fixes; no camera APK, lens mapping or algorithm changes. rc2/rc3 test ZIPs withdrawn.


## v0.7.0-rc3 — prerequisite/policy hotfix candidate

- Refuse activation unless the paired0.113 APK, running LSPosed, enabled module and owner-user OppoCamera scope are verified. Native SQLite access is read-only; no scope edits.
- Remove live custom SELinux type/typeattribute creation. Reuse hal_camera_default_exec and verify its execute_no_trans permission before provider changes.
- Preserve rc2 directory/default-enabled/locking/logging/qualified-auto fixes. Bind auto approval to the actual scripts/helper as well as package/profile/kernel identity.
- Same-boot corrected activation,10-camera health and user photo/viewer passed; no reboot during that test. This does not prove the exact prior reboot cause. Fresh/automatic reboot tests pending.


## v0.7.0-rc2 — hotfix candidate

- Prepare missing app-private camera files using the verified package UID and SELinux context before dependency setup; refuse symlink paths and foreign configuration.
- Serialize Action attempts, keep root-only output and preserve exit codes. Repeated Action verifies active state without restarting the provider.
- Bound camera-service queries and health wait; do not detach access beneath a failed runtime rollback. Owned busy access mounts have a lazy-detach fallback.
- Enable fresh installs for the next boot; preserve disabled updates and the failed-boot guard.
- Add qualified automatic activation: first Action activates, a second Action after a successful photo test confirms the same session, then one background attempt after completed boot +15s. Failure/interruption blocks retries; updates reset approval.
- Keep hook0.113 and original OEM APK/ELFs unchanged.

Confirmed: local first-run repair/photo pass and repeated Action on the development phone; host refusal/rollback/installer fixtures. Not confirmed: cause of reported forced reboot, full rc2 fresh-boot installation, other ROMs or Magisk.

## v0.7.0-rc1 — first experimental public release

Five exact ROM dependency profiles; KSU/Magisk live-policy backends; per-ROM
OpenCL layout selection; manual post-boot activation; transactional stdc++
replacement and health rollback; next-boot disable guard; disabled-by-default
installation. New package paths have host checks, not cross-ROM runtime proof.
Paired hook0.113 retains tested Night/UltraDark, sizes/beauty/AI/WB/time-lapse and
shared isolated SNPE/Symphony fixes. No OEM APK/smali/donor ELF byte edits.
Full functional scopes and quality limits: docs/FEATURE_STATUS.md.
