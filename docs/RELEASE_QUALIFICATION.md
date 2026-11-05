# v0.8.0-rc1 qualification

- Original F.14 camera APK and 418 source payload entries match the pinned donor inventory. Own native hook bytes are unchanged from retained 0.113.
- Test2 lifecycle: automatic boot observed active on the Cyborg A16/KernelSU phone; policy read-back, mount ownership, F14 HAL/CHI, provider, selector, ten-camera enumeration, paired hook/scope and global config check passed. User reports additional reboot tests passed; this is not a quantified stress campaign.
- App namespace: ODM model bind disappeared during app creation; video bokeh crashed after model-read failure. Replacement overlay repair stayed visible and bokeh passed; test2 includes that repair.
- Front Portrait trial 0.120: HAL YUV 4608×3456; two upright JPEGs decode at 3456×4608 (15.925MP). User accepted appearance/blur. No hook-side pixel enlargement, donor edit or native OEM-gate expansion was added. Sensor readout/detail resolution is not proven by dimensions.
- Retained 0.124 removes the temporary flag and diagnostic observer. Cold-reopen Portrait → Photo → Portrait → Night saves all decode at 3456×4608; user confirms effects survive. A separate Front Night Full capture decodes at 2080×4608.
- Final rc1 ZIP uses the test2 boot controls plus paired-hook pin/version/docs changes. The exact final ZIP completed a fresh install/reboot. Startup scope was missing, correctly producing an unconfirmed snapshot. User then enabled scope; live F14 identity, ten cameras, selector, hook scope, policy and mount checks passed and the user confirmed photos/filters. A subsequent boot with scope already configured passed automatic startup: active verdict, all core/extended checks, hook/scope and boot guard passed. No manual activation or repair was performed.
- Original firmware live parity, all mode/lens/ratio combinations, Magisk, other ROMs/kernels and long-session reliability are not certified.

No firmware/kernel flash, wipe or kernel tracing was used for these tests. Public media attachments are not required; the test evidence stays private.

## Transition correction

Full-size Portrait initially exposed a Photo/Portrait pool-reuse failure: original face detection still reported one face, but fb.face.info and beauty JNI metadata updates disappeared after switching to Photo. A bounded diagnostic observer confirmed this. The exact front Portrait main-YUV pool now receives its actual full-reader usage value; the app's original pool handles replacement/listener detachment/closure across modes. No face counts or landmarks are synthesized. The trial survived both switches by user report and original face metadata/JNI updates persisted. Retained 0.124 removes that observer; the final cold-reopen capture/transition smoke passed.
