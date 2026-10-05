# HR5.2 Standard Mode + Ableton Live Validation Milestone

## Current milestone

This note records a separate validation branch from the earlier Mackie Control TX14/TX15 work.

The X-Touch Compact was tested in **Standard Mode**, with the nine faders configured as Pitch Bend on channels 1 through 9. The experimental HR5.2 firmware branch exposes fine fader-position information on transmit, while the Ableton Live Control Surface receives the full Pitch Bend value and writes distinct fine values into the bound SSL Remote parameter.

The important result is that this path now works **end-to-end**:

```text
physical fader
  -> Compact Standard Mode high-resolution PB
  -> USB/CoreMIDI
  -> Ableton Live Control Surface
  -> SSL Remote parameter write
```

The reverse path is deliberately different:

```text
SSL Remote / Live host change
  -> Control Surface host-sync
  -> CH2 / CC1..CC9
  -> Compact motor target
```

That motor-return path is still 7-bit in the current Standard Mode integration and remains the next resolution bottleneck.

## Firmware identity and publication policy

The true-stock application image used as the analysis base is:

```text
size:   52,924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
load:   0x08006000
```

The validated experimental branch is internally identified as **HR5.2 ENDPOINT-GUARD**.

Its observed test-image metadata is:

```text
size:   53,076 bytes
SHA256: 99381e32b4c3aeb343c6b38c06a1662ff4f5b4b97c2c0113ba4943b782015b2d
```

No Behringer firmware image and no experimental firmware binary is distributed by this repository.

## Standard Mode transmit result

Earlier Standard Mode testing showed the same practical coarse behavior as the stock path: useful fader motion was dominated by MSB changes, equivalent to 128 raw Pitch Bend units per coarse step.

HR5 moved the experiment to the complete filtered internal position rather than appending only a recovered low nibble to the coarse output.

HR5.2 keeps that high-resolution path and adds a minimal endpoint guard so the controller does not repeatedly send the same clamped endpoint code while the physical sensor continues to move or jitter beyond the calibrated limit.

The physical validation produced genuine same-MSB changes with non-zero LSB differences. Representative behavior includes transitions with minimum observed deltas of approximately:

```text
33 raw Pitch Bend units
```

compared with the old coarse step of:

```text
128 raw Pitch Bend units
```

This is roughly a 3.9x improvement in observed transmitted granularity on the tested path.

The result should be described as **high-resolution Standard Mode transmission with observed intra-MSB events**, not as full 14-bit mechanical resolution.

## Endpoint and stationary behavior

HR5.2 specifically fixes the endpoint-repeat problem seen in HR5.

A representative upward sequence reached:

```text
16165
16206
16248
16256
```

and then stopped sending repeated identical `16256` values while the fader remained at the top endpoint.

A five-second stationary test after the physical validation produced:

```text
STATIONARY 5s: 0 PB
```

This is the key stability result for the current branch: the fine-resolution path remains active during real movement without producing a continuous idle stream.

The tested Standard Mode upper endpoint is intentionally kept at:

```text
raw14 = 16256
LSB   = 0
MSB   = 127
```

rather than forcing an artificial `16383` endpoint into this path.

## All-nine-fader result

The HR5.2 verifier confirmed intra-MSB activity across all nine faders on the tested unit.

The same validation also preserved the known Standard Mode motor-return mapping:

```text
CH2 / CC1 -> F1
CH2 / CC2 -> F2
...
CH2 / CC9 -> F9
```

Each motor remained individually addressable.

## Ableton Live / SSL Remote validation

The Live validation was performed with:

```text
Ableton Live 12.4.6
macOS 13.7.8 arm64
Apple M1 Pro
Control Surface: SSL_Remote_XTouch_Compact_D8_HOSTSYNC
```

The Control Surface loaded successfully with the X-Touch Compact as both input and output.

The decisive result is that the full incoming Pitch Bend value is preserved by the script rather than being reduced back to the MSB.

Representative F1 telemetry includes:

```text
raw=12768  -> norm=0.748125
raw=12735  -> norm=0.746191
raw=12702  -> norm=0.744258
```

The second and third values remain inside the same broad coarse region while producing distinct normalized writes to the bound `Out Gain` parameter.

Additional representative writes include:

```text
raw=12370  -> norm=0.724805
raw=12337  -> norm=0.722871
raw=12300  -> norm=0.720703
```

This closes the controller-to-host resolution chain for the current Standard Mode branch.

## D8 HOSTSYNC result

D8 adds a host-value shadow/polling path so host-side parameter changes can immediately recall the physical motor position even when the change did not originate from the fader.

The validated Live log includes cases such as:

```text
HOST CHANGE F5 0.394335926 -> 0.749726295
MOTOR CC F5 ch=2 cc=5 value=100
```

and:

```text
HOST CHANGE F2 0.487441421 -> 0.749726295
MOTOR CC F2 ch=2 cc=2 value=100
```

This fixes the earlier behavior where a reset or double-click in the Remote UI could update the parameter but leave the physical motor waiting until another DAW-side nudge occurred.

The anti-loop behavior is also active: Pitch Bend events generated by motor movement while a fader is not touched are detected and rejected rather than being written back into the host parameter.

## Current boundary

The transmit and host-write sides are now high-resolution in the validated Standard Mode path.

The remaining bottleneck is the return path:

```text
Live / SSL parameter
  -> high-resolution internal normalized value
  -> D8 motor feedback
  -> 7-bit CC value on CH2 / CC1..CC9
  -> physical motor
```

For example, a high-resolution internal target around `raw=12795` currently resolves to:

```text
CC value = 100
```

So host-to-motor recall remains quantized to the 7-bit Standard Mode CC path even though controller-to-host control is finer.

This should not be conflated with the separate Mackie Control receive-path investigation documented elsewhere in this repository, where raw Pitch Bend LSB processing and the `112/113` rounding boundary were measured directly.

## What this milestone proves

The current physical + Live evidence supports the following:

1. The Compact can expose stable intra-MSB fader data in the tested Standard Mode branch.
2. HR5.2 suppresses repeated identical endpoint messages while retaining fine movement events.
3. A five-second stationary test produced zero Pitch Bend traffic.
4. All nine faders retain individual Standard Mode motor addressing on CH2 / CC1..CC9.
5. Ableton Live receives full LSB+MSB Pitch Bend values from the Compact.
6. Distinct fine raw values produce distinct SSL Remote parameter writes.
7. D8 HOSTSYNC immediately recalls motor position after host-side parameter changes.
8. Motor-generated PB while the fader is not touched is rejected to avoid feedback loops.
9. The current remaining resolution bottleneck is the 7-bit Standard Mode motor-return path.

## What remains open

The next technical target is isolated from the already validated transmit path:

- determine whether Standard Mode motor feedback can be driven with finer-than-7-bit physical targets without losing the reliable CH2 / CC1..CC9 behavior;
- preserve the current HR5.2 transmit path unchanged while investigating motor return;
- repeat longer stationary and calibration tests across all nine faders;
- validate the same behavior on additional X-Touch Compact hardware/firmware revisions.

---

This document records measurements and integration results only. It intentionally does not document firmware installation, boot-entry, recovery or update procedures, and no firmware binary is distributed here.
