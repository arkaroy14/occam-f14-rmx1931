# Recovery

## If Android works

Disable OcCam F14 in your root manager (KernelSU or Magisk), then reboot once to remove its
mounts. Uninstall afterward. Removal does not try to unmount a running camera stack or apply
policy repairs. The safe order is disable → reboot → uninstall: removal never unmounts
same-boot overlays.

## If Android does not boot

```
adb shell
su
touch /data/adb/modules/occam_f14_fresh/disable
```

KernelSU Next v3.3.0 skips disabled modules in its active-module filter, so the disable marker
also stops the module's `sepolicy.rule` from being loaded at the next boot. **Magisk is
different (v28.1 pinned sources)**: rules live in a pre-init aggregate
(`/data/<preinit>/sepolicy.rule`) that magiskinit loads BEFORE anything can examine
`/data/adb/modules`; `disable_modules()` only creates the marker and unlinks a per-module rule
path that the installer-side aggregator does not write, and no boot stage rebuilds the
aggregate. A disable marker — app-created or recovery-created — therefore does **not** stop
this module's already-cached rules from loading on the following boot; they persist until a
later module install re-runs `copy_preinit_files` (which aggregates enabled modules only,
skipping `disable`/`remove`/`update`-marked trees). The honest escape if the cached rules
themselves are the problem: rebuild that aggregate exactly as `copy_preinit_files` does
(concatenate enabled modules' rule files, skip `disable`/`remove`/`update`-marked trees) —
never delete the whole aggregate, which would silently drop every other module's policies.
This is version-specific (verified against v28.1, commit `218327f9…`) and device-dependent.
Remove the staged update too if one exists: `/data/adb/modules_update/occam_f14_fresh`.

Magisk Safe Mode (v28.1 `check_safe_mode`, evaluated in `post_fs_data` after magiskinit has
already loaded policy) calls `disable_modules()`. That creates disable markers and unlinks
`$MAGISKTMP/<PREINITMIRR>/<module>/sepolicy.rule`. That path is per module. The file magiskinit
reads is the single aggregate `/data/<preinit>/sepolicy.rule` written by `copy_preinit_files`.
Safe Mode does not rebuild that aggregate, so it is not an escape from cached rules. It still
stops later module scripts. Triggers are two consecutive failures to reach boot-complete, the
hardware key combo, or the Magisk app. KernelSU Next reports a kernel safe-mode flag through an ioctl, but
the volume-key trigger on this custom 4.14.357 kernel is **unverified** — do not rely on it as
a guaranteed route. Recovery access/decryption must be established on your device; no wipe is
needed.

## Limits (both managers)

An incomplete boot marker prevents subsequent payload-mount attempts. A failed qualification
disables future attempts and preserves evidence. These checks run **after** the root manager
loads `sepolicy.rule` (Magisk: pre-init by magiskinit; KernelSU Next: `on_post_data_fs` before
module scripts): they cannot contain a fault inside that earlier loader, undo grants already
loaded in the current boot, or guarantee every kernel boots safely. After a ROM switch the
staged rule of the previous ROM can load once before the new tuple is attested and disabled for
following boots. An unexpected reset occurred in a withdrawn late-activation build; the exact
reset cause remains unknown. Our installer only writes the staged tree; note however that the Magisk install wrapper
itself mutates the OLD tree after customize (touches `update`, removes `remove`/`disable`,
copies `module.prop` — v28.1 util_functions.sh ~704–708). A disabled old module stays disabled
across the update because this installer writes `disable` into the STAGED tree (Magisk's own
preserve path reads the old tree, whose marker the wrapper already cleared). Both managers
swap staged updates at the next boot.
