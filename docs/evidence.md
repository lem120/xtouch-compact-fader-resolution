# Evidence and reproduction notes

This document separates **measured facts**, **static-analysis observations**, and **working hypotheses**.

## 1. Raw MIDI — X-Touch Compact, Mackie Control

Representative sequence around the calibrated unity region:

```text
12288  LSB=0  MSB=96
12416  LSB=0  MSB=97
12544  LSB=0  MSB=98
12672  LSB=0  MSB=99
12800  LSB=0  MSB=100
12928  LSB=0  MSB=101
13056  LSB=0  MSB=102
```

Observed ordinary delta: `128` raw units.

Special maximum endpoint observed: `16383`, encoded as `LSB=127, MSB=127`.

## 2. Raw MIDI — X-Touch One control test

Representative values from the same probe path:

```text
12632  LSB=88   MSB=98
12564  LSB=20   MSB=98
12496  LSB=80   MSB=97
12444  LSB=28   MSB=97
12428  LSB=12   MSB=97
```

Non-zero LSB values prove that the capture/decoder path preserves low-bit Pitch Bend data.

## 3. Firmware image identity

Path inside the inspected macOS editor bundle:

```text
X_TOUCH.app/Contents/Resources/UpdateFiles/Xtouch_Compact.bin
```

```text
size:   52924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
```

Do not infer that another Editor release contains the identical image. Compare hashes first.

## 4. Firmware architecture evidence

First two 32-bit little-endian words:

```text
0x200049B8  initial stack pointer
0x0800647D  reset vector (Thumb bit set)
```

The image contains STM32F1-compatible peripheral base addresses including:

```text
0x40012400  ADC1
0x40012800  ADC2
0x40020000  DMA1
0x40021000  RCC
0x40010800  GPIOA
0x40010C00  GPIOB
0x40011000  GPIOC
```

This supports an STM32F1-family target. Exact MCU part number remains to be independently established.

## 5. Nine-channel acquisition path

Around `0x08000562`, a DMA-related routine copies nine halfword values from one buffer to two working buffers and handles a tenth halfword separately.

Key operations:

```asm
ldrh.w  r1, [r2, r0, lsl #1]
strh.w  r1, [r3, r0, lsl #1]
strh.w  r1, [r4, r0, lsl #1]
adds    r0, r0, #1
cmp     r0, #9
blo     loop
```

The source/destination addresses visible in the literal pool include RAM buffers around `0x20002914`, `0x20002928` and `0x20003B50`.

Around `0x080039E8`, the fader processor loops nine times, loads a halfword for each channel, and calls the processing routine at `0x0800381E`.

## 6. 16-sample filter and finer internal value

Inside the routine around `0x0800381E`:

- a halfword sample is inserted into a 16-entry rolling buffer;
- an old sample is subtracted from a running sum;
- the new sample is added;
- the index wraps at 16;
- the code derives both a coarse and finer representation from the sum.

Representative instructions:

```asm
ldrh    r5, [r0, #0x0a]
subs    r1, r1, r5
str     r1, [r4]
strh    r2, [r0, #0x0a]
adds    r1, r0, r2
...
cmp     r0, #0x10
...
ubfx    r0,  r1, #8, #16
ubfx    r10, r1, #4, #16
```

The `>>4` form retains more position detail and is used by adjacent decision/hysteresis logic.

## 7. Mackie Control output path

Around `0x080039A8`, the normal branch contains:

```asm
and     r3, r3, #0x7f
movs    r2, #0x00
movs    r1, #0xe0
movs    r0, #0x0e
bl      0x08004e3e
```

The call site's MIDI behavior is consistent with:

```text
status = 0xE0 + channel
LSB    = 0
MSB    = fader_value & 0x7F
```

The full-scale branch sets `r3=0x7F` and copies it into `r2` before reaching the same message construction, matching the observed `0x7F/0x7F` endpoint.

## 8. Confidence labels

**Measured:** raw MIDI values, LSB/MSB behavior, A/B result.  
**Direct static observation:** file path/hash, vector table, peripheral constants, nine-halfword loops, rolling filter, Mackie output call-site instructions.  
**Inference:** which exact physical ADC channel maps to which fader, exact effective number of noise-free bits, and how much of the internal resolution can be exposed stably.

## 9. Host-to-motor feedback is a separate experiment

The host-to-motor direction must be kept separate from controller-to-host resolution.

In Mackie Control mode, direct raw Pitch Bend tests have independently reproduced a same-MSB boundary where `LSB=112` resolves to the lower 7-bit motor target and `LSB=113` resolves to the next target. Anchored tests reproduced the same boundary at multiple MSB regions, and static disassembly explains the threshold explicitly. See:

- [`docs/motor-feedback-boundary.md`](motor-feedback-boundary.md)
- [`docs/firmware-rx-tx-path.md`](firmware-rx-tx-path.md)

That result does **not** imply 14-bit stable physical motor positioning. It shows that the LSB is processed before a later conversion to a coarser motor target.

In the separate Standard Mode integration used for SSL Remote, the reliable motor-return path is currently:

```text
CH2 / CC1..CC9
```

with one CC number addressing each physical fader individually. That path is currently 7-bit and remains the resolution bottleneck for Standard Mode motor recall.

## 10. Standard Mode HR5.2 + Ableton Live validation

A separate Standard Mode firmware experiment has now reached end-to-end Live validation.

The validated experimental branch is internally identified as **HR5.2 ENDPOINT-GUARD**. Its test-image metadata is recorded for reproducibility, but the image itself is not distributed:

```text
size:   53076 bytes
SHA256: 99381e32b4c3aeb343c6b38c06a1662ff4f5b4b97c2c0113ba4943b782015b2d
```

The important measured result is that the Compact now emits genuine same-MSB Pitch Bend changes in Standard Mode, with minimum observed deltas around:

```text
33 raw units
```

compared with the older coarse step of:

```text
128 raw units
```

HR5.2 also suppresses repeated identical endpoint messages. A representative upward sequence ends at:

```text
16165
16206
16248
16256
```

without continuing to stream duplicate `16256` messages while the fader remains at the top endpoint.

A five-second stationary test produced:

```text
STATIONARY 5s: 0 PB
```

The same physical validation preserved individual Standard Mode motor addressing for all nine faders through `CH2 / CC1..CC9`.

### Ableton Live write-through

With Ableton Live 12.4.6 and the `SSL_Remote_XTouch_Compact_D8_HOSTSYNC` Control Surface, the full incoming Pitch Bend value is decoded and written to the bound SSL Remote parameter.

Representative F1 telemetry includes:

```text
raw=12768 -> norm=0.748125
raw=12735 -> norm=0.746191
raw=12702 -> norm=0.744258
```

and:

```text
raw=12370 -> norm=0.724805
raw=12337 -> norm=0.722871
raw=12300 -> norm=0.720703
```

These are distinct fine writes; the Live path is not collapsing them back to the old MSB-only stream.

### Host-sync result

D8 HOSTSYNC also reacts to host-side parameter changes and immediately issues motor-return CC feedback.

Representative telemetry includes:

```text
HOST CHANGE F5 0.394335926 -> 0.749726295
MOTOR CC F5 ch=2 cc=5 value=100
```

and:

```text
HOST CHANGE F2 0.487441421 -> 0.749726295
MOTOR CC F2 ch=2 cc=2 value=100
```

Motor-generated PB arriving while a fader is not touched is rejected instead of being written back into the host parameter, preventing a feedback loop.

The full milestone is documented in [`docs/hr5-2-standard-mode-live-validation-milestone.md`](hr5-2-standard-mode-live-validation-milestone.md).

### Current boundary

The controller-to-host side is now high-resolution in the validated Standard Mode path. The reverse motor path is still quantized to the 7-bit CC return channel.

This is now the next isolated research target.
