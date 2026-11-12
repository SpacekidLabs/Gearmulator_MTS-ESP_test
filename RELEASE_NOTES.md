Experimental MTS-ESP versions of **NodalRed2x**, **JE8086**, and **OsTIrus**.

Each instrument ZIP contains both **Audio Unit (AU)** and **VST3** plug-ins.
Requires **Apple Silicon**, **macOS 11+**, and a native arm64 DAW. Intel/Rosetta hosts are not supported.

### Install and use

1. Quit your DAW. Copy `.component` bundles to `~/Library/Audio/Plug-Ins/Components/` and `.vst3` bundles to `~/Library/Audio/Plug-Ins/VST3/`.
2. Supply your own compatible firmware; see [firmware requirements and installation](https://github.com/SpacekidLabs/Gearmulator_MTS-ESP_test/blob/main/INSTALL.txt).
3. Restart/rescan the DAW and load the instrument named **MTS Test**.
4. Load an MTS-ESP master and choose a tuning. Chords and held notes follow the global table automatically.

No ROMs or personal presets are distributed. NodalRed2x requires the verified Nord Lead 2X firmware; JE8086 uses JP-8000 v1.05, not JP-8080 rack firmware; OsTIrus was validated with Virus TI2 firmware.

### Scope and validation

One global 128-note tuning table. No per-channel tables, unmapped-key filtering, MTS SysEx input, or DSPBridge tuning. These are experimental, ad-hoc signed builds, not notarized releases. Full musical and DAW regression testing remains incomplete.

All six Release targets built successfully; packaged arm64 binaries and signatures verified. JE8086 tuning checks passed. SHA-256 checksums accompany the downloads.

[Complete corresponding source and build instructions](https://github.com/SpacekidLabs/Gearmulator_MTS-ESP_test/tree/v0.1.0-mts-test/Source) are included in this repository and GitHub's source archives. Gearmulator and third-party licenses/copyright notices are retained. Credits: The Usual Suspects and ODDSound.
