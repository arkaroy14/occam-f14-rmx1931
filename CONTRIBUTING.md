# Reports and contributions

Use GitHub Issues as the durable bug tracker. XDA comments are welcome for
feedback; if a comment becomes a reproducible bug, link it to an issue and avoid
reporting the same issue repeatedly without new evidence.

## Bug report

Supply device/model, exact ROM name/date/build, Android SDK, vendor tree or
firmware base if known, kernel, root manager/version, Zygisk/LSPosed version,
module/hook versions, competing camera modules, steps, frequency and whether
preview or saved output failed. Mention lens, mode, ratio, zoom, light level,
HDR/AI/bokeh/EIS/beauty and recording resolution/FPS where relevant.

Include a short log around the failure, not hours of raw logs. State whether
it is a crash, idle close, frozen preview, processing hang, failed save, colour
fault or playback problem. An UI60fps overlay does not prove encoded60fps;
EXIF or saved dimensions alone do not prove native64MP execution.

## New ROM profile

Same Android or vendor-tree name is not automatic support. Provide exact ROM
ZIP/source links, build date and available diagnostics. A maintainer may request
hashes/dependency information; do not swap drivers, remove identity checks or
flash firmware to make a profile pass. One validated profile can serve multiple
ROM brands sharing the same contract.

## Privacy and safety

Logs and photos can reveal faces, locations, serials, Wi-Fi addresses, account
names and EXIF GPS. Redact them before posting. Images/videos are optional;
share privately only with explicit consent. Never post signing keys or full
personal app-data archives. No kernel probes, dmesg-based probe experiments,
wipes or bootloop reproduction are required for issue reports.

## Patches

Keep original OEM APK/ELF files unchanged. Explain trigger, before/after
behavior, exact scope and rollback. Include meaningful tests and distinguish
host evidence from device evidence. Avoid blanket OEM property spoofing, fake
success callbacks and cosmetic FPS/resolution claims. Follow the selected
source license once finalized; don't copy code or blobs without provenance.
