# Firmware TX/RX path notes

## Image identity

The analysis in this repository refers to the X-Touch Compact firmware image recovered from the X-TOUCH Editor bundle:

```text
filename: Xtouch_Compact.bin
size:     52,924 bytes
SHA256:   7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
base:     0x08000000
```

The repository does **not** redistribute the vendor firmware.

The vector table begins with:

```text
initial SP:   0x200049B8
reset vector: 0x0800647D
```

## Controller -> host: finer data exists before Mackie quantization

The fader-processing routine around `0x0800381E` maintains a 16-sample rolling window. Once populated, the running sum is reduced at two different scales:

```asm
0x0800386E  ubfx  r0,  r1, #8, #16
0x08003874  ubfx  r10, r1, #4, #16
```

The finer `r10` representation is subsequently inspected at nibble precision:

```asm
0x08003944  and   r0, r10, #15
0x08003948  cmp   r0, #8
...
0x0800394E  and   r0, r10, #15
0x08003952  cmp   r0, #8
```

This is evidence that the firmware retains finer position information internally than the ordinary Mackie stream exposes.

### Mackie transmit call site

At the ordinary fader send path:

```asm
0x080039AC  and   r3, r3, #127
0x080039B0  movs  r2, #0
0x080039B2  movs  r1, #224
0x080039B4  movs  r0, #14
0x080039B6  bl    ...
```

The normal message therefore uses:

```text
status/base = 0xE0
LSB         = 0
MSB         = fader_value & 0x7F
```

A separate maximum-value branch sets both data bytes to 127, explaining the full-scale `16383` endpoint without implying continuous 14-bit output.

## Host -> motor: incoming LSB is used only for rounding

The Mackie receive routine begins around `0x08005A42`.

Relevant extraction:

```asm
0x08005A48  ubfx  r0, r0, #8, #8
0x08005A4C  lsrs  r4, r6, #24
0x08005A56  ubfx  r5, r6, #16, #8
```

For Pitch Bend channels E0..E8:

```asm
0x08005A5E  sub.w r1, r0, #224
0x08005A62  cmp   r1, #8
0x08005A64  bhi   other_message
```

Then:

```asm
0x08005A68  mov   r0, r4        ; target = MSB
0x08005A6A  cmp   r5, #112      ; compare LSB
0x08005A6C  bls   keep_target
0x08005A6E  cmp   r0, #127
0x08005A70  bhs   keep_target
0x08005A72  adds  r0, r4, #1    ; LSB > 112
0x08005A74  uxtb  r0, r0
```

Conservative pseudocode:

```c
target = MSB;
if (LSB > 112 && target < 127)
    target = MSB + 1;
```

This exactly matches the experimentally observed 112/113 boundary.

## Combined picture

```text
CONTROLLER -> HOST

analog fader
  -> ADC / rolling acquisition
  -> finer internal position
  -> calibration / hysteresis logic
  -> 7-bit Mackie value
  -> Pitch Bend with LSB forced to 0


HOST -> MOTOR

14-bit Pitch Bend
  -> parse MSB + LSB
  -> if LSB > 112: round MSB upward
  -> 7-bit motor target
  -> servo
```

So there are two distinct firmware bottlenecks:

1. **TX:** finer internal fader information is reduced before transmission and the LSB is explicitly zeroed.
2. **RX:** the incoming low byte is read, but only to round the target into a 7-bit motor command.

## Patch strategy

The lower-risk first experiment is **TX-only**:

- leave motor receive/servo logic unchanged;
- leave acquisition, calibration and existing filtering unchanged;
- expose more of the already available internal fader position in the outgoing Pitch Bend data;
- validate noise, repeatability, endpoint behavior and touch handling before any RX/servo modification.

No experimental firmware should be flashed until the update/recovery path is characterized and a verified fallback exists.
