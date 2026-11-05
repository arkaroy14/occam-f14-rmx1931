# Verification approach and community review

The status table separates host/static evidence, original runtime-call evidence,
decoded saved files and user-visible checks. They answer different questions.
A preview effect, SDK return0 or saved file alone does not prove all downstream
processing is correct. Earlier failed captures are not silently counted as
passes after later repairs.

Development testing included front/rear, four ratios, lens switching, colour
filters, flash/screen light, manual controls, bright/dim/very-dark scenes, white
tube light versus warm/daylight smart light, output decoding, EXIF inspection,
ffprobe/packet timing, native call boundaries, buffer identity observations and
quiet cold Camera restarts after fixes. This was one phone, not a lab-calibrated
comparison with a stock F.14 device. No photos/videos are included publicly.

## Minimal per-profile release check

1. Completed boot and explicit module state; no early HAL activation.
2. LSPosed scope, matched source profile, manual Action, ten cameras and owned
   selector present. Original APK/HAL/CHI hash identities remain expected.
3. RearPhoto1x,ultra-wide andtelephoto: actual new files, correct views/colours.
4. FrontPhoto and frontNight at4:3,1:1,16:9,Full; check saved crops and sizes.
5. Rear ordinaryNight andUltraDark; frontNight with/without screen light.
6. Bokeh Video on/off, EISVideo→Night→Photo; person blur in saved output.
7. A30fps clip,60fps clip,SlowMotion andTime-lapse; decode and measure actual
   timestamps rather than trusting the chosen UI label.
8. Disable returns native ROM camera service, no unowned mount removal, no
   power votes on unqualified profiles. Recovery/boot guard requires a controlled
   test environment; do not deliberately hang a user's daily phone.

## Quality review still useful

Compare natural skin tones, highlight retention, motion ghosts, noise, focus,
detail and depth edges with unchanged framing/light. For Night, keep Tripod,
screen flash and UltraDark hint state recorded. For blur, a real person with a
separate background is a stronger test than a face shown on a flat monitor.
Object cutouts and strong beauty sliders are not calibrated quality benchmarks.

Long sessions, thermal throttling, low storage, background/resume, screen-off,
incoming interruptions, rare stalls and untested mode/ratio/lens combinations
need community testing. Mark results with the exact module/hook/ROM versions.
