# Compatibility and support policy

Device scope: **realme X2 Pro / RMX1931 (samurai), arm64**. Other devices,
including similar OnePlus/OPPO models, are not supported by this package.

| Target | SDK | Camera vendor family | Evidence | Root-manager status |
|---|---:|---|---|---|
| Development phone: InfinityX 3.5, Cyborg Android 16 | 36 | Cyborg/OP4A89-derived pre-overlay pair | Retained camera/hook runtime tested; new 0.7 packaging not installed | KSU Next tested with retained setup; Magisk untested |
| Supplied crDroid 10.11 Android 14 (2025-01-13) | 34 | Same camera pair; Karthik-era ROM | ZIP extraction, hashes and host checks | KSU/Magisk candidate, no boot test |
| Supplied Matrixx 11.5.0 Android 15 (2025-05-23) | 35 | Cyborg | ZIP extraction, hashes and host checks | KSU/Magisk candidate, no boot test |
| Supplied Infinity-X 3.12 Android 16 (2026-07-24) | 36 | Nayem | ZIP extraction, hashes and host checks | KSU/Magisk candidate, no boot test |
| Supplied Infinity-X 4.1 Android 17 (2026-10-25) | 37 | Nayem | ZIP extraction, hashes and host checks | KSU/Magisk candidate, no boot test |

These are five **dependency profiles**, not five individually certified builds.
Full tuple values are in `module/rom-profiles.tsv` and GPU file identities in
`module/gpu-layouts.tsv`. The post-activation donor HAL/CHI remain the F.14 pair.
The pre-overlay ROM pair, metadata shim, stdc++ facade, SDK and GPU layout are
attested. DSP copies were compared against all four supplied ZIPs.

## Similar ROMs

We want one common release, not an archive for every ROM brand. A ROM with the
same validated tuple and interfaces can match an existing profile. Other ROMs
using the same tree may differ in shims, GPU drivers, SELinux, firmware, power
services, provider binary, linker configuration or framework behavior. They need
a compatibility report, not a promise based on the tree name.

A profile refusal is deliberate. Do not remove hash checks or mix blobs to make
it load. Open a ROM-support issue with the exact ROM name/date, SDK, maintainer,
root manager/version, kernel, firmware base if known, and module diagnostic
output. We can add a profile to the same package after validation.

## Runtime requirements still to qualify

Both root managers need compatible Zygisk/LSPosed injection. A KSU pass does not
certify Magisk. Only the development-phone profile attempts the optional finite
CPU/migration performance helper; other profiles leave power settings alone.

Unknown: actual Magisk live policy, early mount/boot-guard behavior on target
builds, sensor/DSP/kernel ABI, neverallow policy, sphal namespace visibility,
hidden APIs, and complete camera regression on each unbooted ROM. Host checks
cannot establish those. No installer test flashes a ROM, kernel or root boot.
