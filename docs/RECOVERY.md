# Recovery and removal

Keep your photos and app data. A module failure does not require wiping the phone or flashing stock firmware.

If Android works, disable OcCam F14 in KernelSU, then reboot once to remove its mounts. Uninstall afterward. Removal does not try to unmount a running camera stack or apply policy repairs.

If Android cannot boot, use an already-working recovery that can access `/data/adb`. Create:

```sh
touch /data/adb/modules/occam_f14_fresh/disable
```

If an interrupted installation left a staged update, inspect `/data/adb/modules_update/occam_f14_fresh` and disable that staged module too before boot. Recovery access/decryption must be established on your device; an unverified volume-key safe-mode gesture is not a guaranteed recovery route. No wipe is needed.

An incomplete boot marker prevents subsequent payload-mount attempts. A failed qualification disables future attempts and preserves evidence. These checks run **after** the root manager loads `sepolicy.rule`: they cannot contain a fault inside that earlier loader, undo grants already loaded in the current boot, or guarantee every kernel boots safely. An unexpected reset occurred in a withdrawn late-activation build; the exact reset cause remains unknown.

Stop after an unexpected reboot. Preserve boot/health logs and available pstore evidence; do not reproduce bootloops, arm kernel tracing probes or repeatedly re-enable the module. Other root-manager modules are outside this package's guard.
