# Experimental compatibility package: recovery and qualification

This package is disabled by default. It is not runtime-qualified on the four
supplied ROMs or Magisk. The installed phone's retained module was NOT changed.
It requires OcCam Fresh LSPosed 0.113 scoped to com.oppo.camera, a working
Zygisk/LSPosed setup for the root manager, and one exact supported ROM tuple.

## Recovery from a boot failure

Do not wipe, format, flash firmware, or delete camera data.

1. Force a restart only if necessary. If the preceding boot did not confirm
   completion, the candidate guard creates `disable` and skips its early mounts
   on the next boot. This guard cannot fix a first boot that is already hung.
2. If recovery or authorized root ADB can access decrypted /data, CREATE or KEEP:
   `/data/adb/modules/occam_f14_fresh/disable`.
   Example: `touch /data/adb/modules/occam_f14_fresh/disable`.
   Root-manager safe-mode/module-disable recovery is another option.
3. Stop and collect logs. Rebooting removes this module's transient mounts and
   live policy additions. Disabling the manager entry alone does not undo all
   mounts/policy in an already-running boot.

Do NOT delete `disable` as a boot-recovery instruction. Deleting it together
with `.boot-pending` is an intentional RETRY after the problem is understood:
`rm /data/adb/modules/occam_f14_fresh/disable /data/adb/modules/occam_f14_fresh/.boot-pending`.
That retry command may report an absent file; check paths manually. Do not run
it until ready to re-enable. No data-loss guarantee or universal recovery claim.

## Activation

After a qualified installation, consciously enable the module and reboot once
on a test setup (not an automatic step). The early APK/config mounts require
boot. Wait for Android to finish booting. Use Action, or a root shell invoking
`sh /data/adb/modules/occam_f14_fresh/action.sh`. HAL activation never runs at boot.
An active old module with the same id must not be treated as a side-by-side test.
Back up that exact package before any replacement; this candidate was not deployed.

Five candidate profiles cover current A16 Cyborg plus A14 Karthik, A15 Cyborg,
A16 Nayem and A17 Nayem supplied ZIPs. Hash refusal means STOP, not remove pins.
Power helper runs only for the measured current-phone profile; other profiles
leave power settings alone. The current phone is a regression target, not an
invitation to reproduce a bootloop. KSU and Magisk are separate environments.

## Required device checks before a supported release

Verify completed boot, profile identity, no early provider reload, Action once,
ten cameras, selector mapped, original APK/HAL/CHI hashes, ordinary photo/video,
Night/UltraDark and bokeh transitions using retained 0.113 routing. A basic photo
alone cannot certify GPU/SNPE branches. Verify disable restores native camera
provider and leaves no owned runtime/access mounts. Failure requires preserving
logs and owned rollback, not widening policies or swapping ROM GPU libraries.

Magisk requires its own live-policy and namespace tests. Mocked command tests
are not real SELinux/mount proof. Never flash a different root boot automatically.


## Same-boot runtime deactivation

If Android still responds, preserve logs first. Ask the maintainer for help if
mount ownership is unclear. The module's runtime disable path restores the
native ROM provider; dependency access disable removes its owned overlays.
Live policy additions last until reboot. A module manager toggle alone does
not guarantee an already-mounted runtime is gone in the current boot.
Do not delete files from /vendor or modify partitions to uninstall this port.

The earlier development trace incident involved kernel probes; public support
will not ask users to arm uprobes/kprobes/tracefs to diagnose this package.
