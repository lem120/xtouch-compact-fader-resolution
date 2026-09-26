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

A community report concerning Ableton Live's `PitchBendFeedbackRule.value_pair_map` suggests that the feedback-map API may expose only integer mapping values from `0` to `127`, potentially limiting that particular feedback path to 128 mapped positions.

This has **not yet been independently reproduced as part of this repository**, so it is tracked as an external observation rather than a measured result. It also does not explain the Compact's controller-to-host measurement, because the Compact's raw USB MIDI stream is already MSB-only before Live receives it.

The clean test is to bypass the feedback-map abstraction and send ordinary Pitch Bend bytes directly while keeping the MSB constant:

```text
raw=12800 -> LSB=0   MSB=100
raw=12816 -> LSB=16  MSB=100
raw=12832 -> LSB=32  MSB=100
...
raw=12912 -> LSB=112 MSB=100
```

Interpretation:

- visible/repeatable motor movement between those values would show that the Compact receive/motor path can respond to low Pitch Bend bits;
- no movement until the MSB changes would indicate an effectively 7-bit receive path as well.

The repository includes `tools/motor-feedback-lsb-test.html` for this experiment.
