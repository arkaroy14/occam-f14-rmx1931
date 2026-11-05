# Installation — experimental testing guide

Read compatibility, feature status and recovery first. The initial 0.7 ZIP is
**disabled by default** and has not been installed on the development phone.
Use only on an environment you are prepared to recover. No data wipe is needed
by design; this is not a guarantee against failures elsewhere in the setup.

## Requirements

- RMX1931, arm64, an exact included Android 14–17 dependency profile.
- Working KernelSU/KernelSU Next OR Magisk. Do not install both together.
- Working compatible LSPosed injection for that root manager.
- The matching root-module ZIP and OcCam Fresh hook APK from one release.
- A way to disable a module through the manager, root ADB or recovery with /data
  available. Keep your current module APK/ZIP versions for rollback.

## Before replacing another camera port

Record existing module and LSPosed scope settings. Avoid simultaneous camera
payload modules or competing hooks. Disable conflicting old ports deliberately;
do not clear camera data or delete your photos. If a newer OppoCamera data
update exists, record it and follow maintainer-assisted removal of that update
while preserving data. A matching APK hash in /data/app still may bypass the
system-path OEM bootstrap. Do not blindly uninstall the system camera.

## Candidate procedure

1. Read the release's paired versions and verify `SHA256SUMS`.
2. Install the **hook APK** normally, then enable **OcCam Fresh** in LSPosed for
   `com.oppo.camera` only. Keep obsolete OccamHook/competing camera hooks off.
3. Install the **root-module ZIP** through the active root manager while Android
   is booted. Recovery ZIP installation is refused by this candidate.
4. The candidate includes `disable`. Enabling and the first reboot are explicit
   test steps, not performed by the installer. Review recovery before enabling.
   Some manager versions may handle initial disabled state differently; verify
   it rather than assuming it. Do not replace a working module casually.
5. After a deliberate test reboot, wait for Android to fully boot. Check LSPosed
   is active. Run the module's **Action**. On a manager without that button,
   an authorized root shell can run:

   ```sh
   sh /data/adb/modules/occam_f14_fresh/action.sh
   ```

6. Action attests the profile, prepares access and manually activates F.14. It
   should report the 10-camera donor stack. Profile refusal: stop and report;
   do not bypass it or reboot repeatedly.
7. Open OppoCamera from its launcher icon. Test ordinary Photo first, then the
   minimal regression checklist in `TESTING.md`. Share results through Issues.

HAL activation is intentionally manual after every boot. Early APK/config
mounts still occur when enabled; the boot guard does not make them risk-free.
Never install a different OppoCamera update as a troubleshooting shortcut.
