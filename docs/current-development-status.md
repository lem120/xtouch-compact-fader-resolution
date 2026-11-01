# Current Compact development status

Updated 1 November 2026. This page describes development results, not a public firmware release.

## Consolidated baseline

The maintained controls baseline is internally called **TEST03c**. A previous internal distribution renamed that same image `Xtouch_Compact_v1.0.0.bin`; comparison of both files gives the same bytes:

- Size: **54,080 bytes**.
- SHA-256: `7607ea9ae518b0e591bceb901d4e430610863fd72169ea7e802214e1ba06e2d8`.
- Host-owned relative encoders require the **D10f-or-later compatible protocol**, including the D10g ring-pattern work.
- These names describe development artifacts; they are not Behringer version numbers or an existing GitHub download.

This identity differs from both the stock image and the earlier HR5.2 image documented in the research notes. Do not interchange their offsets, hashes, preparation instructions or rollback assumptions.

## Relative encoder protocol

After host ownership is established by feedback, the sixteen encoders transmit:

| Field | Value |
|---|---|
| MIDI channel | 16 (status `BF`) |
| CC number | 42–57, one per encoder |
| Delta | `value - 64` |
| Positive movement | 65–127 |
| Negative movement | 1–63 |
| Neutral | 64 |

Examples from CPU-emulated execution of the firmware sender:

- `BF 2A 41`: encoder 1, delta +1.
- `BF 2A 3F`: encoder 1, delta −1.
- `BF 39 44`: encoder 16, delta +4.

These are **emulated sender samples**, not new physical MIDI captures. The firmware's circular descriptor no longer limits parameter travel. The host applies each delta to the actual current parameter value, clamps to that parameter's range and responds immediately when rotation reverses at a limit.

Before the first host feedback and after standalone restart, the profile/legacy behavior remains in use. The new relative messages belong to encoders, not faders. A legacy absolute-counter driver must not be assumed compatible with the host-owned relative phase.

## Ring and button workflow

The developed firmware path preserves the rotation-related flicker correction. Host-side feedback chooses patterns and values. In the validated console workflow:

- Pan has a center reference and directional fill.
- Q uses a symmetric fan pattern.
- Gain, frequency and release use fill behavior determined by their assigned range/reference.
- Layer A/B supports logical bank navigation without using it as an unwanted MIDI-layer switch.
- SHIFT/FLIP, selection and mute/solo/arm depend on the receiving driver's mapping.

PAN/WIDTH/POSITION, plugin selection and DAW actions are host integrations. They are not advertised as universal standalone firmware features.

## Evidence and confidence

| Evidence | Scope |
|---|---|
| Existing TX14/TX15 notes | Physical Mackie Control experiments; historical transmit grid |
| Existing HR5.2 milestone | Standard Mode physical and Live captures, all-nine-fader checks and host-sync observations |
| TEST03c development verification | 4,368 CPU-emulated cases: 512 buttons, 1,936 receive cases, 192 ring patterns, 1,152 relative rotations and 576 legacy-sender comparisons |
| D10f verification | 68 simulated-host driver tests, including firmware-emitted relative samples fed into the receiver |
| D10g + Remote integration | Maintainer-reported physical acceptance of encoder travel, ring behavior and the console workflow on the development unit |

The TEST03c automated run explicitly did **not** exercise physical USB transport or perform a new hardware validation. The maintainer's subsequent physical acceptance is a separate source of evidence. The summary in [captures/test03c-validation-summary.json](../captures/test03c-validation-summary.json) preserves this distinction.

Host editions for 8/16/24/32 targets have been developed; channel counts and bank navigation are software capacities. Automated checks of those editions do not imply a new physical validation of every size on every Compact revision.

## Before public download

The development work supports preparing a free release. Public availability remains pending the exact supported package, version naming, profile/driver dependencies, updater instructions and recovery documentation. Additional hardware-revision validation is still useful.

CONSOLE 4000 E Remote remains a related application project, not a prerequisite for understanding or reproducing the measurements in this repository.
