# Source and binary provenance

## What this project authors

Compatibility scripts, LSPosed Java/C++ hook logic, owned provider adapters,
profile selection, validation tests and release documentation. Publishing that
source does not publish or relicense OEM algorithms.

## Firmware and binary sources

| Component | Source/provenance | Role and limit |
|---|---|---|
| OppoCamera / OEM framework / HAL / CHI / configs | Recovered RMX1931 realme UI 2.0 F.14 firmware dump | Main unchanged binary donor; firmware archive is not OEM source code |
| Development ROM files and behavior | Current Cyborg-based Android 16 phone and recorded Cyborg device/vendor trees | Tested platform; not interchangeable with every Cyborg ROM |
| Imported Ultra Dark runtime pair | Current ROM / recorded OP4A89-derived dependency lineage | Not authenticated as shipped in recovered F.14; algorithm/stock-quality equivalence not proved |
| Ancillary ROM DSP/GPU/shim/stdc++ variants | Explicit development-phone or supplied-ROM profiles | Keep ROM-specific drivers; byte identity and runtime qualification are separate |
| Nayem profiles | Supplied A16/A17 ZIPs and Nayem vendor tree | Compatibility targets, not primary F.14 donor |
| C.38 / F.11 packages | Earlier firmware comparison research | Research references; not blanket donor replacements in this release |

The app APK is byte-identical to original F.14; this does not make the whole
runtime pure F.14. Imported dependencies and our hooks must remain disclosed.
Some algorithms are closed-source; SDK return0 is not proof of processing
quality or stock parity. Every release should ship a path/hash/origin manifest
with proprietary licensing scope, without developer home paths or device IDs.

## Pinned original identities

- OppoCamera: `3b299c9c0f2d790038f7ea52e10f5bae424e096b339502f6c2c1bb4a3831acd5`
- HAL: `847b1f9e19ef20e58cdf7ac42b0d3ed2c026b1e1b343be28471019c5de8e4ac0`
- CHI: `a0836c593f66580a7faef35463abf7bd5a049b0acb91b72a6ef16a6fbd397fce`
- Owned retained selector: `3eb0986349165e98c855c712efe6445b84515b0f1eaea4f752dd954ac2d928bd`

Public provenance covers selected payload identities; it must not imply every
closed SDK was independently validated or that vendor Git availability grants
redistribution permission. Firmware locations and checksums used internally
must be reviewed for public redistribution before attaching the full ZIP.

## Earlier research material

Earlier handoff projects were consulted as background while investigating the
port. This does not establish that their patched APKs or implementations were
incorporated into the retained runtime. No specific retained code or algorithm
is attributed to those handoffs without an identified source path. The OEM APK
used here is the unchanged F.14 binary; no claim of complete independence from
prior research is made. Any later identified adopted logic needs explicit credit.
