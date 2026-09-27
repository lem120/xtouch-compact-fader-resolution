# Motor-feedback same-MSB boundary

## Scope

This note documents the host-to-motor side of the X-Touch Compact fader-resolution investigation.

The controller-to-host path is a separate result: in normal Mackie Control travel, the Compact transmits Pitch Bend with `LSB=0` and effectively 7-bit position steps. Here we ask a different question: **does the Compact use the low Pitch Bend byte when receiving motor-position feedback?**

## Test environment

- X-Touch Compact firmware: **1.14**
- Mode: **Mackie Control**
- Fader/Pitch-Bend channel: **1**
- Transport: raw Web MIDI Pitch Bend messages
- DAW feedback-map abstractions: bypassed

Pitch Bend is decoded as:

```text
raw14 = LSB + (MSB << 7)
```

The tests repeatedly retransmit each target at 20 Hz and return to a known baseline between trials.

## Why same-MSB tests matter

A comparison such as `12800 -> 12928` changes the MSB from 100 to 101, so a moving motor does not tell us whether the low byte matters.

The decisive tests instead compare values inside one MSB bucket:

```text
12912 = LSB 112 / MSB 100
12913 = LSB 113 / MSB 100
```

Only the LSB changes, by one raw 14-bit count.

## Lower-state threshold test (R5)

Starting from the lower state around raw `12800`, the exact threshold scan tested `LSB 112, 113, 114, 115, 116` while keeping `MSB=100`.

| Raw target | LSB | MSB | Observed motor state |
|---:|---:|---:|---|
| 12912 | 112 | 100 | lower |
| 12913 | 113 | 100 | upper |
| 12914 | 114 | 100 | upper |
| 12915 | 115 | 100 | upper |
| 12916 | 116 | 100 | upper |

## Upper-state return test (R8)

The fader was armed at `12927 = LSB 127 / MSB 100` and stepped downward through the same MSB bucket.

| Raw target | LSB | MSB | Observed motor state |
|---:|---:|---:|---|
| 12927 ... 12913 | 127 ... 113 | 100 | upper |
| 12912 | 112 | 100 | lower |

## Current interpretation

At this tested operating point:

```text
12912 -> lower motor state
12913 -> upper motor state
```

These targets differ by exactly one raw 14-bit count while the MSB remains unchanged.

> The X-Touch Compact firmware 1.14 motor-feedback receive path processes information from the Pitch Bend LSB deeply enough to distinguish adjacent same-MSB target values at the tested operating point.

This does **not** imply 16,384 stable physical motor positions. The evidence is compatible with LSB-aware target processing followed by a coarser internal conversion/quantization stage.

## What remains open

- how many distinct stable motor positions exist across the full travel;
- whether the same `112/113` boundary repeats in other MSB regions;
- whether the boundary varies with calibration or fader position;
- how this receive-side conversion relates to the MSB-only controller-to-host output routine.

## Next experiment

Repeat the adjacent-count boundary search around several other MSB regions, for example 90, 99, 101 and 110.
