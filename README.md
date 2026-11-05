# Stock F.14 OPPO Camera for realme X2 Pro — OcCam

Original realme UI 2.0 F.14 OppoCamera for RMX1931 custom ROMs, using systemless dependencies and scoped LSPosed compatibility hooks. Maintained by **Arka Roy Chowdhury**.

**v0.8.0-rc1 is an experimental release candidate.** Pair the root-module ZIP with **OcCam Fresh 0.124**. Install, enable the hook for OppoCamera, reboot and open Camera. There is no Action step, second confirmation or hook-app superuser grant.

This candidate accepts only the measured **Cyborg-based Infinity-X 3.5 / Android 16 / KernelSU Next** dependency profile. Four other Android 14–17 profiles remain reference data; they are not enabled support. Magisk is refused pending qualification. Earlier broad compatibility drafts do not describe this package.

The original signed camera APK and donor camera ELF files are unchanged. Our Java/native hooks and provider adapters change runtime compatibility behavior; this is not an unchanged stock OS or a guarantee of stock image quality.

## Downloads and setup

Use the matching ZIP and hook APK from [Releases](https://github.com/arkaroy14/occam-f14-rmx1931/releases). GitHub's automatic source ZIP is not installable. Do not install a newer OppoCamera APK over F.14.

- [Install and update](docs/INSTALL.md)
- [Compatibility](docs/COMPATIBILITY.md)
- [Feature status and quality limits](docs/FEATURE_STATUS.md)
- [Recovery and removal](docs/RECOVERY.md)
- [Release qualification](docs/RELEASE_QUALIFICATION.md)
- [Build notes](docs/BUILD.md), [provenance](docs/PROVENANCE.md), [credits](CREDITS.md)
- [Report bugs or request ROM support](CONTRIBUTING.md)

## What changed

Early module preparation replaces the withdrawn late Action-based activation. Startup reports active, inactive or unconfirmed status, with specific hook/LSPosed scope instructions when needed. The notification is a startup snapshot.

The test2 lifecycle booted with all ten cameras and the F.14 stack active. Replacement overlays preserve camera models inside the app namespace. Front Portrait now receives a 4608×3456 YUV stream and two saved upright JPEGs were verified at 3456×4608, instead of square 1728×1728 output. A 16MP file is not independent proof of sensor readout/detail resolution. Retained hook 0.124 passed cold reopen, Portrait ↔ Photo effects/saves and Front Night Full (2080×4608). See qualification notes for the final packaged candidate's test boundary.

## Known limits

Native 64M full-RDI proof, exact 60fps delivery, 480fps frame gaps, rare preview/capture stalls and broader quality review remain open. EIS strength, segmentation edges, difficult scenes and long-session thermals need user review. Frame controls in additional modes were not forced visible: their original final UI behavior remains unverified.

Old rc2–rc7 development candidates are withdrawn. An unexpected reset occurred during the former late policy-loading path; its exact kernel cause was not established. Normal early policy loading also precedes the module's guard. Read recovery instructions before testing.

The repository contains our sources and module controls, not proprietary OEM camera source, firmware dumps, signing keys or private test media. See [license status](LICENSE_STATUS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
