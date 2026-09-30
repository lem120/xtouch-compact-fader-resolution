# TX15 Physical + Ableton Live Validation Milestone

## Current milestone

TX15 advances the X-Touch Compact experiment from **effective 11-bit physical MIDI output** to the first successful **end-to-end use of that fine position data inside Ableton Live / SSL Remote**.

The core TX14 result is preserved:

```text
LSB = 0, 8, 16, 24, 32, 40, 48, 56,
      64, 72, 80, 88, 96, 104, 112, 120
```

which corresponds to:

```text
7 MSB bits + 4 recovered fine bits = 11 effective source bits
2048 transmitted position codes
raw14 = fine11 << 3
```

TX15 keeps the same completed-frame sampling point and the same 11-bit output lattice, but adds a one-code stateful Schmitt band around the last transmitted fine code.

## Why TX15 was needed

TX14 solved the earlier replay of intermediate values from the 16-sample processing window, but a physical stationary test exposed a residual adjacent-code oscillation such as:

```text
raw14 120 <-> 128
```

Those values differ by exactly one fine code, or 8 raw Pitch Bend units.

TX15 changes only the local fine-domain state behavior. For ordinary non-endpoint values:

```text
candidate == last       -> suppress
|candidate - last| == 1 -> suppress
candidate >= last + 2   -> transmit candidate - 1
candidate <= last - 2   -> transmit candidate + 1
```

This creates a one-code hysteresis band without coarsening the transmitted lattice back to 16, 32 or 128 raw units.

Exact bottom and top remain priority endpoints.

## Physical TX15 result

A direct MIDI capture from the physical test unit shows the expected fine lattice in real hardware.

Near the bottom of travel the stream includes:

```text
E0 68 00 -> raw14 104
E0 70 00 -> raw14 112
E0 78 00 -> raw14 120
E0 00 01 -> raw14 128
E0 08 01 -> raw14 136
E0 10 01 -> raw14 144
E0 18 01 -> raw14 152
E0 20 01 -> raw14 160
```

The exact endpoints are also present:

```text
bottom: E0 00 00 -> raw14 0
top:    E0 7F 7F -> raw14 16383
```

Slow movement around the working region retains ordered 8-raw-unit fine steps across MSB boundaries. One captured sequence includes:

```text
12400
12408
12416
12424
12432
12440
12448
12456
12464
12472
12480
12488
12496
12504
...
12528
12536
12544
```

The sustained two-code boundary chatter seen with TX14 is not present in this TX15 validation capture. This is evidence that the Schmitt strategy is operating as intended on the tested unit.

It is not yet evidence that every physical position on every fader is equally noise-free.

## First end-to-end Ableton Live validation

The next important result is that the recovered fine position is no longer confined to a standalone MIDI monitor.

With the TX15-aware SSL Remote Control Surface in Ableton Live, the incoming full Pitch Bend value is decoded and written to the bound SSL parameter while a real fader touch gesture is active.

Captured Live telemetry includes consecutive fine-grid values such as:

```text
raw14=12912
raw14=12920
raw14=12936
raw14=12944
```

and:

```text
raw14=11896
raw14=11888
raw14=11880
```

Each value is passed through as a distinct normalized parameter write rather than being reduced back to the old MSB-only 7-bit stream.

This closes an important part of the resolution chain:

```text
physical fader
  -> Compact firmware fine position
  -> TX15 11-bit Pitch Bend grid
  -> USB/CoreMIDI
  -> Ableton Control Surface
  -> SSL Remote parameter write
```

## 16-channel Remote binding result

The same integration has also reached successful 16-channel binding in SSL Remote.

The current Control Surface detects a 16-slot Remote protocol and associates all sixteen SSL Remote Engine targets:

```text
protocol slots=16
16/16 SSL associati. CORE pronto.
```

This does not mean sixteen physical faders exist on the Compact. The hardware remains eight channel faders plus Master, with software banking used to address the larger Remote.

It does prove that the fine-resolution Compact input path and the 16CH Remote binding architecture can coexist in the same working Live session.

## What this milestone proves

The current physical + Live evidence supports all of the following:

1. The Compact can transmit a repeatable effective 11-bit fader-position grid in Mackie Control mode.
2. TX15 preserves that grid while adding local adjacent-code hysteresis.
3. Exact bottom and top Pitch Bend endpoints remain available.
4. The pathological sustained TX14 adjacent-code chatter is absent in the TX15 validation capture.
5. Ableton Live can receive the full TX15 Pitch Bend value and use distinct fine codes for SSL Remote parameter writes.
6. The same integration can bind a 16-slot SSL Remote configuration successfully.

## What remains open

The following are still separate validation targets:

- long-duration stationary testing at many positions across the full travel;
- repeatability and noise characterization on all nine Compact faders;
- calibration consistency between individual faders;
- motor-feedback behavior across the full fine grid;
- final controller ergonomics: encoder pickup, LED-ring state, banking, contextual controls and shutdown lifecycle;
- validation on additional X-Touch Compact hardware/firmware revisions.

The current wording remains **effective 11-bit transmitted position**, not “true 11-bit mechanical resolution” and not “14-bit fader resolution.”

---

This note intentionally records only the technical milestone and observed result. It does not document firmware installation, boot-entry, recovery or update procedures, and no firmware binary is distributed here.
