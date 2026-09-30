# TX14 Physical Validation Milestone — Effective 11-bit Fader Output

## Current milestone

A later experimental X-Touch Compact firmware build (TX14) has now been validated on physical hardware with direct MIDI monitoring in Mackie Control mode.

TX14 changes the sampling point used by the experimental transmit path so that the fine position is evaluated only after completion of the stock 16-sample processing window, rather than exposing intermediate values from inside that window.

The resulting Pitch Bend stream now shows a repeatable fine-position grid with sixteen distinct LSB substeps per MSB:

```text
LSB = 0, 8, 16, 24, 32, 40, 48, 56,
      64, 72, 80, 88, 96, 104, 112, 120
```

This corresponds to an **effective 11-bit transmitted position** mapped into the standard 14-bit MIDI Pitch Bend format:

```text
7 MSB bits + 4 recovered fine bits = 11 effective bits
2048 digital position codes
raw14 = fine11 << 3
```

The MIDI transport remains 14-bit, but the three least-significant transport bits do not currently carry additional source information.

## Physical evidence

Slow sweeps show ordered fine steps while the MSB remains unchanged. For example, one captured region traverses:

```text
MSB 98:
LSB 0
LSB 8
LSB 16
LSB 24
...
LSB 112
LSB 120
```

and then crosses continuously into:

```text
MSB 99 / LSB 0
MSB 99 / LSB 8
MSB 99 / LSB 16
...
```

The same fine grid is observed in both directions of travel.

This is materially different from the earlier TX11 result, where the LSB was active but did not provide a stable absolute fine-position subdivision.

## What TX14 fixes

TX12/TX13 exposed fine information too early in the processing path and caused repeated transmission of values belonging to the internal sample window.

TX14 moves the experimental decision point to the completed processing frame. The previous high-rate replay pattern is no longer present.

The result is a substantially cleaner stream whose changes track actual fader movement rather than repeatedly replaying intermediate history.

## Remaining limitation: adjacent-code chatter

TX14 still exposes a small amount of fine-position jitter when the physical fader rests close to a boundary between adjacent 11-bit codes.

A representative stationary case alternates between:

```text
raw14 120  = E0 78 00
raw14 128  = E0 00 01
```

These two values are exactly one fine step apart:

```text
128 - 120 = 8 raw Pitch Bend units
```

Similar one-step oscillation can occur at other positions.

This is now consistent with boundary jitter in the consolidated fine measurement, rather than the earlier internal-buffer replay problem.

## Next research target

The next revision should preserve the TX14 sampling point and 11-bit grid while adding a small stateful hysteresis around adjacent fine codes.

The intended behavior is:

- retain 8-raw-unit output granularity during real movement;
- suppress repeated one-step boundary chatter while the fader is stationary;
- preserve the exact full-scale endpoints;
- avoid falling back to the original 128-raw-unit coarse resolution.

The current working target is therefore **stable effective 11-bit output**, not 14-bit source resolution.

## Interpretation boundary

The TX14 result demonstrates approximately 2048 distinct **digital transmit codes** across the mapped range.

It does **not** by itself prove 2048 mechanically stable or noise-free physical positions of the fader. Mechanical repeatability, analog noise, calibration and servo behavior remain separate measurements.

---

This note intentionally records only the technical milestone and observed physical result. It does not document firmware installation, boot-entry, recovery, or update procedures, and no firmware binary is distributed here.
