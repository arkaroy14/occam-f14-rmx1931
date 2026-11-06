# Compatibility

Device: **realme X2 Pro / RMX1931 (samurai), arm64 only**. Similar OPPO/OnePlus devices are not supported.

Two separate statuses are always distinguished: **eligible** (the installer and boot controller
accept the tuple/manager combination and prepare it correctly) and **device-qualified** (that
combination actually booted and passed camera checks). Only one environment is device-qualified.

| Environment | Eligibility | Device qualification |
|---|---|---|
| Infinity-X 3.5 / Cyborg Android 16, exact measured tuple (`a16_cyborg_live`) + KernelSU Next 3.3.0 on the measured custom 4.14.357 kernel | Eligible (full measured policy: provider self-exec + both measured DSP allows, 8 atomic checks) | **Scoped device pass**: v0.8.1-rc1 owner fresh-install/restart/camera checks; earlier unexplained hang disclosed in qualification |
| KernelSU Next on other kernels/builds | Eligible; no version floor is claimed | Not qualified |
| Magisk (module script + `sepolicy.rule` support is standard Magisk behavior) | Eligible, experimental | Not qualified |
| crDroid 10.11 Android 14, Karthick-era snapshot (`a14_host_candidate`) + either manager | Eligible; retained vendor policy corroborates the referenced types (names proven present in that snapshot) | Not qualified |
| Matrixx 11.5.0 Android 15, Cyborg snapshot (`a15_host_candidate`) + either manager | Eligible; full or core policy decided by the live install-time capability probe on that ROM | Not qualified |
| Infinity-X 3.12 Android 16, Nayem snapshot (`a16_host_candidate`) + either manager | Eligible; same live capability decision | Not qualified |
| Infinity-X 4.1 Android 17, Nayem snapshot (`a17_host_candidate`) + either manager | Eligible; same live capability decision | Not qualified |

Policy staging is decided by a **live, read-only capability probe at every install**
(`root-preflight policy-capability`). It scans the provider-exec query and both DSP groups.
A completed query is either allowed (`0`) or denied (`31`). A denial is expected before the
manager applies the staged rule and is not an unsupported ROM. Exit `0` stages the full
three-allow rule. Exit `33` means the probe ran and exec completed, but one or both DSP
groups were unverifiable: the installer stages only the packaged allow lines whose groups
completed and names the missing group. Exit `32` means the probe itself failed (missing API
or not enforcing) or the provider-exec context is unverifiable: install refuses. The boot
read-back checks exactly the staged lines. A core or partial rule is not full-camera
eligibility, and the reporter will not call it active. `rom-profiles.tsv`'s `dsp_policy`
column is informational only. A retained vendor `sepolicy.cil` and `file_contexts` can show
type names in a snapshot. They do not prove merged-policy behavior, installed labels, or
working DSP algorithms, and they are not the only input required for feature compatibility.

Magisk eligibility additionally requires the preinit rule cache: v28.1 `copy_preinit_files`
(util_functions.sh 570–586) writes `$MAGISKTMP/.magisk/preinit/sepolicy.rule` and its caller
does not check the return, so the installer aborts when that directory is absent (rules could
never be applied).

Both managers load the staged `sepolicy.rule` **before** module scripts (Magisk: pre-init by
magiskinit; KernelSU Next: in `on_post_data_fs` before module scripts), so the boot guard cannot
contain a failure inside that earlier loader on either manager, and one boot of already-loaded
grants cannot be undone after a ROM switch. Same Android version, vendor-tree lineage or
maintainer name is not sufficient; kernel/SELinux/linker/GPU/provider behavior must also be
qualified on device.

A compatible ROM brand may share an existing tuple after verification. Report exact
ROM/date/SDK, root manager, kernel, vendor sources and profile diagnostics. Do not remove hash
checks to force a match.

This candidate does not include the earlier optional performance helper. Previous automatic
CPU/migration vote experiments are not a current shipping feature.
