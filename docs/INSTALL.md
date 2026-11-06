# Installation and update

## Requirements

- realme X2 Pro / RMX1931, arm64, matching one of the five attested dependency tuples in
  `rom-profiles.tsv`. The installer checks the complete tuple; a similar ROM name is
  insufficient.
- KernelSU Next **or** Magisk, with working modules and compatible Zygisk/LSPosed. The measured
  environment is KernelSU Next 3.3.0 on the custom 4.14.357 kernel; Magisk and other
  kernels/builds are install-eligible but not device-qualified. No manager version floor is
  claimed beyond each manager's standard module/`sepolicy.rule` support.
- Matching module ZIP and OcCam Fresh hook 0.124. Verify SHA256SUMS.
- Enough storage for the downloaded ZIP and expanded payload (roughly 1.3 GiB together, plus
  working headroom). Do not install with storage nearly full.
- Know how to disable a module if Android cannot boot, on YOUR manager (KernelSU: module
  disable; Magisk: module disable or Safe Mode). Recovery must actually access /data/adb. No
  wipe is required.

## Fresh installation

1. Remove conflicting camera modules. Never install an updated OppoCamera over the bundled F.14 APK.
2. Install OcCam Fresh 0.124 APK. In LSPosed, enable it and select **OppoCamera (`com.oppo.camera`)**, not OcCam Fresh itself. If OppoCamera is not listed yet, complete the next steps and select it after the first boot.
3. Install the matching root ZIP through KernelSU or Magisk. The installer attests the tuple and runs a read-only live policy capability probe. A completed probe stages the full measured rule, or only the allow lines whose contexts the live policy could answer, and names any missing DSP group. A failed probe refuses the install. On Magisk the preinit rule cache must be present or the installer aborts. Confirm the module is enabled before reboot. An intentionally disabled older installation remains disabled during an update until you enable it (this installer writes `disable` into the staged tree; Magisk's own wrapper clears the old tree's marker after `customize.sh`).
4. Reboot once. After Android starts, read the startup notification. Open OppoCamera manually. **No Action button or second activation confirmation exists.** The hook app does not need a superuser grant.
5. If notification asks for LSPosed setup, enable the paired hook and select OppoCamera, then close/reopen Camera. This changes injection scope, not the runtime mounts. Check the diagnostic log if startup itself also failed.
6. Take a rear Photo, open its thumbnail and verify that the actual file saves. Test modes you use and report reproducible faults. If the startup report says the policy mode is core or partial, the named missing grant is unqualified — report what works, and do not treat that boot as full-camera eligibility.

## Updating from any active older OcCam runtime

Disable the old camera module and reboot to the ROM stack before installing this candidate. The installer attests the original ROM dependencies; it must not be bypassed while an older F.14 overlay is active. Install the paired hook/ZIP, explicitly enable the staged update if its disabled state was preserved, then reboot once. Keep Camera data/photos; do not clear them as an update step.

## Status

The startup notification distinguishes active/inactive/unconfirmed and names a missing hook, stopped LSPosed or missing OppoCamera scope. It reports one startup check, not continuous health or every camera feature, and the health log records the attested profile, policy scope and manager backend. If you add the scope after that check, the earlier notification remains unchanged for that boot; close/reopen Camera to load the hook, and the next boot recomputes startup status. A stale setup notice alone is not a runtime failure. Logs are `/data/adb/occam_f14_fresh_boot.log` and the module's `health.last.log`.

If it reports failure, preserve logs and disable the module; do not repeatedly reboot a failing setup or bypass profile/policy checks. Do not grant extra broad SELinux rules or swap vendor libraries. See [recovery](RECOVERY.md).
