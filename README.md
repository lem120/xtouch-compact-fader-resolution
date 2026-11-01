# X-Touch Compact — High-Resolution Firmware Project

**Finer fader input, continuous relative encoders and host-driven LED feedback for the Behringer X-Touch Compact.**

An independent Daily Saver Tools project that started with raw MIDI measurements and developed into a custom firmware workflow tested with a matching Ableton Live control-surface driver.

> **Status — 1 November 2026:** development baseline validated by the maintainer on the test unit; public download being prepared. **No firmware binary is currently published in this repository or its Releases.** This is an unofficial project, not a Behringer firmware release.

[Development status](docs/current-development-status.md) · [Download status](DOWNLOADS.md) · [Research archive](docs/research-history.md) · [Contributing](CONTRIBUTING.md)

## What the project improves

| Area | Result in the developed workflow | Dependency / boundary |
|---|---|---|
| Fader input | Fine Pitch Bend data reaches the host instead of being reduced to the coarse MSB-only value | Standard Mode profile and matching host receiver; MIDI transport width is not mechanical resolution |
| Encoders | Relative changes continue in either direction without an absolute 0–127 counter stopping the assigned parameter early | The relative protocol becomes active after host feedback; requires a compatible driver |
| LED rings | Host feedback drives the rings; the rotation-related flicker was corrected in the tested firmware path | The host decides the displayed parameter value and ring pattern |
| Layer A/B | Buttons can navigate logical fader banks while the intended hardware layer remains selected | Custom firmware behavior plus host bank logic |
| Buttons and focus | SHIFT/FLIP, selection and mixer actions form a working control workflow | The receiving driver assigns the actions; firmware alone does not add DAW commands |
| Motor recall | The driver recalls faders after host changes and avoids feeding motor motion back as user input | Calibration, touch handling and the host feedback path matter |

The currently documented development baseline is **TEST03c**, internally packaged as **v1.0.0** with the same bytes. That internal version label is **not a published GitHub release**. See the [baseline identity and validation boundaries](docs/current-development-status.md).

## Firmware, driver and application are different layers

- **Firmware** runs inside the Compact and controls the MIDI/feedback behavior available to a host.
- **A compatible driver or MIDI integration** interprets that behavior and assigns software actions.
- **The controlled application** supplies parameter values, bank contents and feedback.

Installing firmware alone does not provide a complete Ableton mapping, console UI or plugin control system. The 8/16/24/32-channel editions developed alongside this work are host configurations, not four different hardware capacities.

The proposed free download will document the matching profile, required host components and tested behavior. A standalone before-host-feedback path exists, but compatibility with arbitrary DAWs or existing mappings is not claimed.

## Download

**Public firmware download: in preparation.**

See [DOWNLOADS.md](DOWNLOADS.md) for the publication status and intended package contents. Published assets will be listed on the repository's [Releases page](https://github.com/lem120/xtouch-compact-fader-resolution/releases).

The repository's **Code → Download ZIP** downloads documentation and measurement tools; it is not a firmware installer. No download button here points to an unpublished asset.

## How we got here

The investigation began with a coarse motor-fader feel around unity. Raw captures outside Ableton showed that ordinary stock Mackie Control output used a zero Pitch Bend LSB. An X-Touch One provided an A/B control, and static analysis identified the firmware's transmit quantization path.

Subsequent work separated the two directions: **fader → host** input and **host → motor** recall. Improving one does not prove identical resolution in the other.

| Milestone | Evidence / significance |
|---|---|
| Stock MIDI and motor-return investigation | [Evidence](docs/evidence.md), [112/113 motor boundary](docs/motor-feedback-boundary.md), [RX/TX disassembly](docs/firmware-rx-tx-path.md) |
| TX14 / TX15 in Mackie Control mode | [TX14](docs/tx14-11bit-physical-validation-milestone.md), [TX15](docs/tx15-11bit-live-validation-milestone.md): effective 11-bit transmit grid on the tested path |
| HR5.2 in Standard Mode | [Physical + Live milestone](docs/hr5-2-standard-mode-live-validation-milestone.md): all-nine-fader input checks, endpoint behavior and host-sync; motor-return quantization remains a separate limit |
| Later controls and TEST03c | [Current status](docs/current-development-status.md): relative encoders, ring feedback and matching driver integration |

The detailed original article remains available in the [research archive](docs/research-history.md). Its historical milestone labels do not replace the current status above.

## Validation boundaries

The maintainer has confirmed the consolidated workflow on the development Compact. CPU-emulation and simulated-host checks supplement those physical tests; they do not establish compatibility with every hardware revision, every DAW or every updater environment.

We do **not** claim 16,384 mechanically stable fader positions, improved servo resolution simply because MIDI Pitch Bend uses 14-bit messages, or universal compatibility. A public package needs an exact firmware identity, installation/profile instructions and a documented recovery procedure for its supported configuration.

## Related project

**CONSOLE 4000 E Remote**, formerly SSL Remote, is the separate console-control project from which this investigation grew. It supplies a particular host workflow; it is not the firmware itself. Its presentation and distribution will remain separate and will be linked here when the public project URL is available.

## Contributions

Raw MIDI captures and repeatable tests on additional Compact revisions are welcome. Include firmware identity, mode/profile, driver version, host version, fader or encoder index and the exact sequence tested. See [CONTRIBUTING.md](CONTRIBUTING.md).

Vendor firmware binaries are not stored in this repository. The existing preferred publication model is user-side patching of an identified original image; the final free-download format has not yet been published. See [firmware notes](firmware/README.md).
