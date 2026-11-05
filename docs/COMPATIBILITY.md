# Compatibility

Device: **realme X2 Pro / RMX1931 (samurai), arm64 only**. Similar OPPO/OnePlus devices are not supported.

| Environment | Current v0.8.0 candidate |
|---|---|
| Infinity-X 3.5 / Cyborg Android 16, exact measured dependency tuple | Enabled; final rc1 fresh boot and subsequent automatic active boot verified |
| KernelSU Next 3.3.0 on the measured custom 4.14.357 kernel | Tested environment; other builds/kernels need qualification |
| Magisk | Installer refuses; new lifecycle not qualified |
| crDroid 10.11 Android 14, Karthick-era snapshot | Reference tuple retained; installer refuses |
| Matrixx 11.5.0 Android 15, Cyborg snapshot | Reference tuple retained; installer refuses |
| Infinity-X 3.12 Android 16, Nayem snapshot | Reference tuple retained; installer refuses |
| Infinity-X 4.1 Android 17, Nayem snapshot | Reference tuple retained; installer refuses |

The five rows in rom-profiles.tsv record dependency research. They are **not five enabled/boot-qualified targets**. Both installer and boot controller restrict this candidate to `a16_cyborg_live`, SDK36 and its measured GPU layout. Same Android version, vendor-tree lineage or maintainer name is not sufficient.

A compatible ROM brand may share an existing tuple after verification. Report exact ROM/date/SDK, root manager, kernel, vendor sources and profile diagnostics. Do not remove hash checks to force a match. Kernel/SELinux/linker/GPU/provider behavior must also be qualified.

This candidate does not include the earlier optional performance helper. Previous automatic CPU/migration vote experiments are not a current shipping feature.
