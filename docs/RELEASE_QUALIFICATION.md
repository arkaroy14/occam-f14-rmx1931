# v0.8.1-rc1 qualification

- Original F.14 camera APK and 418 source payload entries match the pinned donor inventory. Own native hook bytes are unchanged from retained 0.124.
- Cross-root restoration (corrected candidate): the installer accepts the five attested tuples on KernelSU Next and Magisk, probes the target's LIVE policy capability read-only at install (full rule when every required query completes; only the completed allow lines when a DSP group is unverifiable; refuse when the probe fails or provider-exec is unverifiable), and the boot controller derives the gate class from the staged rule itself, then emits a root-owned current-boot witness consumed by the reporter (no post-overlay native-ROM re-attestation). Core and partial modes are not reported active. Host results are in `TEST_RESULTS.md` beside this candidate. Host fixtures alone are not a device pass.
- Earlier v0.8.0-rc1 device qualification: only the measured Cyborg A16 + KernelSU Next environment completed fresh-boot, subsequent-active-boot, camera and Portrait/Night size verification. Every other tuple/manager combination is install-eligible and explicitly NOT device-qualified; Magisk lifecycle facts are cited from pinned official docs, not tested on this kernel.
- Test2 lifecycle: automatic boot observed active on the Cyborg A16/KernelSU phone; policy read-back, mount ownership, F14 HAL/CHI, provider, selector, ten-camera enumeration, paired hook/scope and global config check passed. User reports additional reboot tests passed; this is not a quantified stress campaign.
- App namespace: ODM model bind disappeared during app creation; video bokeh crashed after model-read failure. Replacement overlay repair stayed visible and bokeh passed.
- Front Portrait full 4:3 output and pool-usage correction retained (retained 0.124): cold-reopen Portrait → Photo → Portrait → Night saves all decode 3456×4608; Front Night Full decodes 2080×4608.
- Original firmware live parity, all mode/lens/ratio combinations, Magisk and other ROMs/kernels on device, and long-session reliability are not certified.  DSP-dependent capture is unqualified on ROMs whose live policy cannot express the DSP grants (named prerequisite at install).

No firmware/kernel flash, wipe or kernel tracing was used for these tests. Public media attachments are not required; the test evidence stays private.

## Exact cross-root ZIP owner tests

The owner reports successful fresh-install and repeated restart/camera tests of this exact v0.8.1-rc1 ZIP on the measured Cyborg Android 16 / KernelSU Next phone. One earlier second-boot logo hang recovered after a TWRP reboot and did not recur in the later owner tests. Its cause remains unresolved; storage was suggested but not proven. No quantified stress count or other-ROM/Magisk device qualification is claimed.

Current recovered-boot checks independently showed the full F.14 stack active, all ten cameras, hook/scope and matching current-boot witness. The incomplete previous KernelSU log does not establish a cause for the earlier hang. No change to the ZIP was made after testing.

## Transition correction

Full-size Portrait initially exposed a Photo/Portrait pool-reuse failure: original face detection still reported one face, but fb.face.info and beauty JNI metadata updates disappeared after switching to Photo. A bounded diagnostic observer confirmed this. The exact front Portrait main-YUV pool now receives its actual full-reader usage value; the app's original pool handles replacement/listener detachment/closure across modes. No face counts or landmarks are synthesized. Retained 0.124 removed that observer; the final cold-reopen capture/transition smoke passed.
