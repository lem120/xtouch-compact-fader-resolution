# Motor-feedback same-MSB boundary

## Scope

This note documents the host-to-motor side of the X-Touch Compact fader-resolution investigation.

The controller-to-host path is a separate result: in normal Mackie Control travel, the Compact transmits Pitch Bend with `LSB=0` and effectively 7-bit position steps. Here we ask the opposite question: **does the Compact use the low Pitch Bend byte when receiving motor-position feedback, and where is that information reduced?**

## Test environment

- X-Touch Compact firmware: **1.14**
- Mode: **Mackie Control**
- Fader/Pitch-Bend channel: **1**
- Transport: raw Web MIDI Pitch Bend messages
- DAW feedback-map abstractions: bypassed

```text
raw14 = LSB + (MSB << 7)
```

## R5 / R8: exact same-MSB boundary at MSB 100

The decisive comparison is:

```text
12912 = LSB 112 / MSB 100
12913 = LSB 113 / MSB 100
```

From the lower state, 112 remains low and 113 moves high. From the upper state, 113 remains high and 112 returns low.

The MSB does not change, so this proves that the receive path is not simply discarding the low byte.

## R9: useful but reset-confounded multi-region pass

R9 repeated the 112/113 test around MSB 90, 99, 101 and 110 using only the local same-MSB LOW/HIGH span as the reset mechanism.

That pass exposed a methodological issue: at some positions the 127-count local span was not sufficient to guarantee a reproducible physical starting state. R9 is therefore retained as an intermediate test, not as the final multi-region result.

## R10: anchored multi-region validation

R10 uses external hard anchors only to establish direction/state:

```text
HARD LOW  -> local LOW  -> same-MSB target
HARD HIGH -> local HIGH -> same-MSB target
```

The decisive target itself remains entirely inside the selected MSB bucket.

Tested regions:

```text
MSB 90
MSB 99
MSB 101
MSB 110
```

The observed boundary is the same in every tested region:

| Direction | LSB 112 | LSB 113 |
|---|---|---|
| From lower state | lower state | next state |
| From upper state | previous state | upper state |

The corresponding raw logs are in:

- `captures/motor-feedback-lsb-r9-multi-region-report.txt`
- `captures/motor-feedback-lsb-r10-anchored-hysteresis-report.txt`

## Firmware result: the boundary is explicit

The exact firmware image analyzed is:

```text
size:   52,924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
```

The firmware itself is not redistributed by this repository.

In the Mackie Pitch Bend receive routine around `0x0800BA42`, the relevant path extracts:

- MIDI status from bits 8..15;
- LSB from bits 16..23;
- MSB from bits 24..31.

For Pitch Bend channels E0..E8, the decisive sequence is:

```asm
0x0800BA68  mov   r0, r4
0x0800BA6A  cmp   r5, #112
0x0800BA6C  bls   keep_target
0x0800BA6E  cmp   r0, #127
0x0800BA70  bhs   keep_target
0x0800BA72  adds  r0, r4, #1
```

Interpreted conservatively:

```c
target = MSB;
if (LSB > 112 && target < 127)
    target++;
```

That exactly predicts the R5/R8/R10 boundary:

```text
LSB 112 -> current 7-bit target
LSB 113 -> next 7-bit target
```

For non-saturated values it is equivalent in result to `(raw14 + 15) >> 7`, although that is **not** the literal instruction sequence used by the firmware.

## Interpretation

The receive path therefore does process the LSB, but only to round the incoming 14-bit Pitch Bend target into a coarser 7-bit motor target.

That is different from full 14-bit motor positioning:

```text
14-bit MIDI target
        ->
LSB-aware rounding
        ->
7-bit motor target
        ->
servo control
```

The number of stable physical positions across the complete fader travel remains a separate question.

## Relationship to the transmit path

On controller-to-host output, the firmware does the opposite kind of reduction: it has a finer internal fader representation available, but the normal Mackie send path around `0x080099AC` constructs Pitch Bend with `LSB=0`.

See `docs/firmware-rx-tx-path.md` for the TX/RX path side by side.
