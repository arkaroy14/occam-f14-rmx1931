#!/system/bin/sh
# Print-only recovery guidance. Module removal never unmounts same-boot overlays
# (measured in the 2026-11-05 incident), so the safe order is:
#   1. disable the module in the manager,
#   2. reboot once (the manager then skips this module's sepolicy.rule and scripts),
#   3. uninstall.
echo 'OcCam F14 simple-boot: prefer disable -> reboot -> uninstall.'
echo 'This uninstaller performs no mount or policy operations.'
