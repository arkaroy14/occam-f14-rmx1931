# Credits and contribution scope

Credits identify actual contributions; they do not imply endorsement or grant
permission to redistribute proprietary files.

## Main donor and platform authors

- **realme / OPPO** — realme UI 2.0 F.14 firmware, the original signed
  OppoCamera APK, OEM framework/configuration/tuning and camera components.
  The OEM APK and donor camera ELF bytes are preserved. F.14 is a proprietary
  binary/firmware donor, not an open-source camera implementation.
- **Qualcomm and the other named component owners** — camera HAL/CHI, GPU/DSP,
  SNPE/Symphony and algorithm components retain their owners' rights/notices.
  ArcSoft and other SDK components are not claimed as our code.
- **Android/AOSP and ROM platform contributors** — framework and platform APIs.

## Development ROM and device lineage

- [cyborgdc2000](https://github.com/cyborgdc2000) — the Cyborg device/vendor work
  used by the development phone and the environment on which runtime validation
  was performed. [Device tree](https://github.com/cyborgdc2000/android_device_realme_samurai),
  [vendor tree](https://github.com/cyborgdc2000/proprietary_vendor_realme_samurai).
- [karthick111](https://github.com/karthick111) and
  [HyperTeam](https://github.com/HyperTeam) — earlier RMX1931 device/vendor/kernel
  lineage. In particular, the camera-stack update from OP4A89 in Cyborg's vendor
  history is authored by karthick111:
  [commit 8ede335](https://github.com/cyborgdc2000/proprietary_vendor_realme_samurai/commit/8ede3352114b8fc3fbc65c614e720bd2cf6f1aa3).
  This credits that inherited work; it does not attribute every current port
  fix to the team or claim every old component was copied from them.

## Additional compatibility targets

- [nayem8854](https://github.com/nayem8854) — supplied Android 16/17 device/vendor
  sources and ROM snapshots inspected to add compatibility profiles.
  [Device tree](https://github.com/nayem8854/android_device_realme_samurai),
  [vendor tree](https://github.com/nayem8854/proprietary_vendor_realme_samurai).
  This is compatibility-target/provenance credit. The port was developed and
  tested on the Cyborg phone, not represented as built on Nayem's ROM work.

## Port development and testing

- **Arka Roy Chowdhury** — project maintenance, integration decisions and hands-on device
  testing across modes, lighting, ratios, captures and recordings.
- AI-assisted development/research used Codex/ChatGPT, Grok, Gemini/Argon,
  Claude and OpenCode sessions. Their analyses were treated as hypotheses until
  checked; tool use is not evidence that an algorithm is correct. Human device
  testing and source-bound checks determine the stated qualifications.

## Tools and upstreams

- [LSPosed](https://github.com/LSPosed/LSPosed) and Xposed API — scoped runtime
  injection and documented native-hook interface; do not infer a bundled hook
  engine implementation from the API reference.
- [Magisk](https://github.com/topjohnwu/Magisk) / topjohnwu and contributors —
  module format, policy tooling and the included official installer bootstrap.
- [KernelSU](https://github.com/tiann/KernelSU), KernelSU Next and contributors —
  root/module environment and the development phone's root manager.
- Gradle, Android SDK/NDK, LLVM, FFmpeg/ffprobe, Pillow, REA/Ghidra and their
  contributors — build, static analysis and verification tooling. Tool packages
  are not all bundled or runtime dependencies.

See `THIRD_PARTY_NOTICES.md` for redistribution/license scope. A link or credit
is not a claim that upstream supports this package.
