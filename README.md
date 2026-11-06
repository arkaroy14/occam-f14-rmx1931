# Stock F.14 OPPO Camera for realme X2 Pro — OcCam

Original realme UI 2.0 F.14 OppoCamera for RMX1931 custom ROMs, using systemless dependencies and scoped LSPosed compatibility hooks. Maintained by **Arka Roy Chowdhury**.

**v0.8.1-rc1 is an experimental cross-root pre-release.** Pair the root-module ZIP with **OcCam Fresh 0.124**. Install, enable the hook for OppoCamera, reboot and open Camera. There is no Action step, second confirmation or hook-app superuser grant.

## Downloads and setup

Use the matching ZIP and hook APK from [Releases](https://github.com/arkaroy14/occam-f14-rmx1931/releases). GitHub's automatic source ZIP is not installable. Do not install a newer OppoCamera APK over F.14.

- [Install and update](docs/INSTALL.md)
- [Compatibility](docs/COMPATIBILITY.md)
- [Feature status and quality limits](docs/FEATURE_STATUS.md)
- [Recovery and removal](docs/RECOVERY.md)
- [Release qualification](docs/RELEASE_QUALIFICATION.md)
- [Build notes](docs/BUILD.md), [provenance](docs/PROVENANCE.md), [credits](CREDITS.md)
- [Report bugs or request ROM support](CONTRIBUTING.md)

## Features at a glance

**Tested on the development realme X2 Pro: Cyborg Android 16 + KernelSU Next, hook 0.124.** Earlier mode checks are retained across releases; the latest root ZIP received fresh-install/restart/camera checks. This is not a fresh retest of every row on every ROM.

**✅ Tested** = the listed function worked in our checks. **⚠ Limited / Open / Review** = a known limit, unresolved report or incomplete verification. **⏳ Pending** = device testing still needed. A pass does not guarantee stock quality in every scene.

| Feature | Status | What was checked / limit |
|---|---|---|
| Rear Photo: main / ultra-wide / telephoto | ✅ Tested | Distinct views and photos save. |
| Photo zoom up to 20× | ✅ Tested | Clean saves; high zoom is digital, not 20× optical. |
| Front Photo + four frame ratios | ✅ Tested | Full-size 4:3 and cropped ratios save correctly. |
| 64M capture + AI | ⚠ Limited | Saves cleanly; AI preview works. Native full-resolution sensor path still unproven. |
| Rear Night + Ultra Dark | ✅ Tested | Saved colours passed tested desk/dim/dark scenes; broader quality review remains. |
| Front Night + four frame ratios | ✅ Tested | Larger output and crops save. Near-zero light needs screen light; RAW remains binned. |
| Rear Tripod Night | ✅ Tested | Countdown, processing and saved photo checked. |
| Front / rear Portrait blur | ✅ Tested | Face/background blur works; front output is full 4:3. Edge/object quality needs review. |
| Front beauty: Photo / Portrait / Night | ✅ Tested | Eye/slim effects checked; Photo ↔ Portrait switches retain effects. Not every slider measured. |
| Front / rear video bokeh | ✅ Tested | Person clips save/play with visible blur; object/edge quality needs review. |
| Rear Photo AI: all four ratios | ✅ Tested | Live colour/contrast effect and saves checked. |
| Rear HDR ON / Auto; front Auto HDR | ✅ Tested | Scene-dependent activation and saved-photo checks passed. |
| Expert: main / ultra-wide / telephoto | ✅ Tested | Lens views, saves, WB and manual-control checks passed. |
| Expert RAW + zoom | ✅ Tested | Main-camera DNG + JPEG verified. RAW hides zoom; RAW off offers up to 5×. |
| Rear 1080p30 / 4K30 video | ✅ Tested | Clips save/play in tested configurations; long recordings not qualified. |
| Front 1080p30 + video AI beauty | ✅ Tested | Save/play and available AI beauty effect checked. |
| Rear 1080p60 / 4K60 | ⚠ Limited | Saves/plays; ~57 fps cases and gaps observed. Perfect sustained 60 fps unresolved. |
| Slow Motion 240 | ⚠ Limited | Saves/plays; physical capture-rate accuracy not independently certified. |
| Slow Motion 480 | ⚠ Limited | Saves/plays, but measured batch-sized frame gaps remain. |
| Slow Motion 960 option | ⚠ Limited | Processed burst saves/plays; not native 960 fps proof. |
| Front / rear Time-lapse | ✅ Tested | Default speed saves/plays; retained timing correction measured. Long runs untested. |
| UltraSteady / EIS | ⚠ Limited | Records/saves; transitions tested. Stabilization strength still needs user review. |
| Ultra-wide video | ✅ Tested | Wide-view 1080p30 recording checked; other resolutions/lens handovers not all tested. |
| Flash / torch / selfie screen light | ✅ Tested | Rear flash/torch and front Photo/Night illumination checked. |
| Photo / video colour filters | ✅ Tested | Front/rear saved grayscale checks passed; not every filter checked. |
| Panorama / Ultra Macro | ✅ Tested | Panorama opens; close-up Macro focuses and saves. Quality needs wider review. |
| Text Scanner / AI text hint | ✅ Tested | Document crop/straightening saves; OCR/text copying not claimed. |
| Timer / watermark / rotation / viewer | ✅ Tested | Tested countdown, orientations, controls and thumbnail/viewer work. |
| Rare Photo / Portrait / UI stalls | ⚠ Open | Intermittent reports remain; not reliably reproduced or certified fixed. |
| Automatic boot startup | ⚠ Review | Later fresh-install/restart tests passed. One earlier unexplained logo hang remains on record. |
| Other ROM profiles / Magisk | ⏳ Device tests pending | Five exact tuples and both root paths eligible; device tests only on Cyborg A16 / KSU Next. |

[Detailed mode evidence](docs/FEATURE_STATUS.md) · [ROM compatibility](docs/COMPATIBILITY.md) · [Boot qualification](docs/RELEASE_QUALIFICATION.md)

All five exact Android 14–17 dependency tuples are eligible on KernelSU and Magisk paths, subject to live library/GPU and policy capability checks. Magisk also requires its writable pre-init rule cache. Full, partial and core policy states are explicit; partial/core states cannot report the runtime fully active. The exact new ZIP passed owner fresh-install/restart/camera checks on the measured Cyborg Android 16 / KernelSU Next phone. Other profiles and Magisk remain device-unverified.

The original signed camera APK and donor camera ELF files are unchanged. Our Java/native hooks and provider adapters change runtime compatibility behavior; this is not an unchanged stock OS or a guarantee of stock image quality.


## What changed

Early module preparation replaces the withdrawn late Action-based activation. Startup reports active, inactive or unconfirmed status, with specific hook/LSPosed scope instructions when needed. The notification is a startup snapshot.

The test2 lifecycle booted with all ten cameras and the F.14 stack active. Replacement overlays preserve camera models inside the app namespace. Front Portrait now receives a 4608×3456 YUV stream and two saved upright JPEGs were verified at 3456×4608, instead of square 1728×1728 output. A 16MP file is not independent proof of sensor readout/detail resolution. Retained hook 0.124 passed cold reopen, Portrait ↔ Photo effects/saves and Front Night Full (2080×4608). See qualification notes for the final packaged candidate's test boundary.

## Boot qualification

The owner reports successful fresh-install and repeated restart/camera tests of this exact v0.8.1-rc1 ZIP on the measured Cyborg Android 16 / KernelSU Next phone. One earlier second-boot logo hang recovered after a TWRP reboot and did not recur in the later owner tests. Its cause remains unresolved; storage was suggested but not proven. No quantified stress count or other-ROM/Magisk device qualification is claimed.

## Known limits

Native 64M full-RDI proof, exact 60fps delivery, 480fps frame gaps, rare preview/capture stalls and broader quality review remain open. EIS strength, segmentation edges, difficult scenes and long-session thermals need user review. Frame controls in additional modes were not forced visible: their original final UI behavior remains unverified.

Old rc2–rc7 development candidates are withdrawn. An unexpected reset occurred during the former late policy-loading path; its exact kernel cause was not established. Normal early policy loading also precedes the module's guard. Read recovery instructions before testing.

The repository contains our sources and module controls, not proprietary OEM camera source, firmware dumps, signing keys or private test media. See [license status](LICENSE_STATUS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
