# OcCam F14 for realme X2 Pro

Original realme UI 2.0 F.14 OppoCamera, adapted for RMX1931 custom ROMs with
systemless dependency overlays and scoped LSPosed compatibility hooks.

**Experimental pre-release candidate.** The retained camera build has been
tested on one Cyborg-based Android 16 phone. The new root-module package adds
host-validated profiles for four more supplied ROMs and KSU/Magisk backends;
those new configurations have not been boot-tested. This is not an official
realme/OPPO release or a promise of stock-camera quality in every mode.

The original signed `com.oppo.camera` APK and donor camera ELF files are unchanged
on disk. Our own hook APK, provider adapters, namespaces and runtime responses
change compatibility behavior. This is not an untouched stock OS environment.

## Start here

- [Installation and requirements](docs/INSTALL.md)
- [ROM and root-manager support](docs/COMPATIBILITY.md)
- [Mode-by-mode verification and quality limits](docs/FEATURE_STATUS.md)
- [Recovery and removal](docs/RECOVERY.md)
- [Report a bug or request a ROM profile](CONTRIBUTING.md)
- [Credits](CREDITS.md) and [source/binary provenance](docs/PROVENANCE.md)
- [Source and build notes](docs/BUILD.md)

## Downloads

Use the [Releases page](https://github.com/arkaroy14/occam-f14-rmx1931/releases): one matching root-module ZIP and the separate
**OcCam Fresh LSPosed APK**. The hook APK is our app; the proprietary OppoCamera
APK is part of the donor payload and retains its original ownership.
GitHub's automatic **Source code.zip** is not an installable root module.
Both components are required. Do not install a newer OppoCamera APK over F.14.

Current candidate: root module `0.7-host-candidate`, hook `0.113-shared-snpe`.
The pair is explicitly documented in each release; versions are separate.

## What users can expect

On the development phone, Photo/lens switching, front capture and ratios,
64M saving, Night/Ultra Dark colour fixes, face beauty, manual Expert controls,
portrait/video bokeh, time-lapse and several other paths have functional passes.
See the table for exact scopes. Native 64M readout proof and perfect high-frame-
rate delivery remain open. Rare preview/capture stalls still need user review.

The compatibility work preserves each ROM's GPU driver files and uses explicit
identity profiles. A similar ROM may work after verification; the same Android
version, vendor fingerprint or maintainer name alone is not sufficient.

## Development

The repository contains module scripts, the hook app's Java/C++ sources,
provider adapter sources, profile tables, host tests and packaging recipes.
It does not provide proprietary OEM camera source code. Firmware blobs, test
photos/videos, personal logs, firmware archives and signing keys are not source
repository content. See [license status](LICENSE_STATUS.md).
