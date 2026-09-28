# X-Touch Compact Fader Resolution Investigation

> **Research status:** reproducible measurements + preliminary static firmware analysis.  
> **Scope:** measurements, protocol observations and firmware-level notes.  
> **This repository does not distribute Behringer firmware or an experimental firmware image.**

## The short version

While developing a high-resolution control workflow for motorized faders, we found a repeatable behavior on the **Behringer X-Touch Compact**:

- in **Mackie Control mode**, the faders are transmitted as MIDI Pitch Bend messages;
- the message format is nominally 14-bit, but over almost the entire travel the Compact sends **LSB = 0** and only changes the MSB;
- this produces effective increments of **128 raw Pitch Bend units**;
- the same test path, using an **X-Touch One**, produces non-zero LSB values and much finer increments;
- static analysis of the X-Touch Compact firmware bundled with X-TOUCH Editor shows that the controller keeps more position information internally than it exposes in its normal Mackie Control fader output;
- the Mackie Control output routine explicitly constructs the ordinary fader message with a zero low data byte and a 7-bit fader value in the high data byte, with a special full-scale case.
- on the **host -> motor** path, the Compact can distinguish two consecutive 14-bit Pitch Bend targets while the MSB remains unchanged: `LSB=112` resolves to the lower 7-bit motor target while `LSB=113` resolves to the next target;
- anchored multi-region tests reproduced the same `112/113` boundary at **MSB 90, 99, 101 and 110**;
- static disassembly now explains that boundary exactly: the Mackie receive routine compares the Pitch Bend LSB with `112` and increments the MSB-derived motor target only when `LSB > 112`.

The current evidence therefore points to **quantization in the firmware/output path**, rather than a fundamental 7-bit limitation of the motor fader itself.

This matters because it suggests that a higher-resolution output path may be technically possible. That remains an experimental question: electrical noise, calibration, filtering, motor control and bootloader/recovery behavior still need to be characterized before any firmware modification can be considered reliable.

---

## Why we started looking

This investigation grew out of **SSL Remote**, a project that maps console-style channel-strip control to Ableton Live using motorized control surfaces.

The practical problem was simple: on the X-Touch Compact, an SSL-style output gain control felt too coarse around unity. A single physical fader step could produce roughly a few tenths of a dB of change. The first assumption was that the limitation might be in Ableton Live, our Control Surface script, the plug-in parameter mapping, or our calibration code.

So we stopped guessing and measured every layer independently.

---

## 1. What the Compact actually sends

In Mackie Control mode, the Compact sends fader position using MIDI Pitch Bend messages. A standard Pitch Bend message carries two 7-bit data bytes:

```text
status = 0xE0 + channel
LSB    = data byte 1
MSB    = data byte 2
raw14  = LSB + (MSB << 7)
```

Around our calibrated unity point, the Compact repeatedly produced sequences like:

```text
raw=12288  lsb=0  msb=96
raw=12416  lsb=0  msb=97
raw=12544  lsb=0  msb=98
raw=12672  lsb=0  msb=99
raw=12800  lsb=0  msb=100
raw=12928  lsb=0  msb=101
raw=13056  lsb=0  msb=102
```

Every ordinary step is exactly 128 raw units:

```text
128 = 1 << 7
```

So the transport is a 14-bit Pitch Bend message, but the useful position information behaves like a 7-bit source placed in the MSB.

At the absolute top end, the Compact can emit `16383` (`LSB=127, MSB=127`). That is a special endpoint behavior and does not represent continuous 14-bit resolution across the travel.

### We repeated the test outside Ableton

The same behavior was measured using a standalone Web MIDI raw probe, with Ableton removed from the signal path. That ruled out Live and our SSL Remote script as the source of the quantization.

We also tested the Compact in Standard mode with a fader explicitly configured as Pitch Bend. The useful values were still 7-bit in practice.

---

## 2. A/B test: X-Touch One vs X-Touch Compact

The strongest control test was to run an **X-Touch One** through the same raw MIDI probe.

The One produced values such as:

```text
raw=12632  lsb=88   msb=98
raw=12564  lsb=20   msb=98
raw=12496  lsb=80   msb=97
raw=12444  lsb=28   msb=97
raw=12428  lsb=12   msb=97
...
```

The LSB is clearly active, and observed increments include values as small as 16 raw units.

That A/B test is important because it validates the complete measurement chain:

```text
controller -> USB/CoreMIDI -> browser/Web MIDI -> decoder
```

The probe is capable of seeing low-bit fader information when the controller sends it.

### Result so far

| Test | X-Touch Compact | X-Touch One |
|---|---:|---:|
| Message type | Pitch Bend | Pitch Bend |
| LSB active during normal travel | No | Yes |
| Typical minimum observed raw step | 128 | down to 16 in our sample |
| Ableton required for result | No | No |

This does **not** by itself tell us whether the Compact is limited by its fader, ADC, MCU or firmware. For that, we had to go deeper.

---


## 3. Host-to-motor resolution: the LSB is actually processed

A separate question is what happens in the opposite direction:

```text
host -> USB MIDI -> X-Touch Compact -> motor target
```

Some Ableton Remote Script users report that `Live.MidiMap.PitchBendFeedbackRule.value_pair_map` behaves like a mapping table with values in the integer range `0..127`. That API-level observation is relevant to Live integrations, but it is **not** the same question as the native resolution of MIDI Pitch Bend or the Compact motor receive path.

For this investigation we therefore bypassed Live's feedback-map abstraction and sent raw two-byte Pitch Bend values directly to the Compact.

### 3.1 First result: same-MSB LSB-only changes move the motor

With firmware **1.14**, Mackie Control mode, fader/PB channel 1, the initial test compared:

```text
12800 = E0 00 64   (LSB   0, MSB 100)
12927 = E0 7F 64   (LSB 127, MSB 100)
```

The motor moved even though the MSB never changed. A normal one-MSB control step, `12800 -> 12928`, produced a comparable movement. This ruled out the simple hypothesis that the Compact always discards the Pitch Bend low byte on receive.

However, an LSB staircase did not produce a clean continuum of intermediate physical positions. That led to a series of threshold tests rather than a claim of full 14-bit motor positioning.

### 3.2 Exact same-MSB boundary at this operating point

The decisive tests kept **MSB = 100** throughout and examined adjacent 14-bit target values.

From the lower state:

```text
12912 = LSB 112 / MSB 100  -> stays low
12913 = LSB 113 / MSB 100  -> moves high
```

From the upper state, armed at `12927` while still keeping `MSB = 100`:

```text
12913 = LSB 113 / MSB 100  -> stays high
12912 = LSB 112 / MSB 100  -> returns low
```

So the observed boundary is the same in both directions:

```text
12912 -> lower motor state
12913 -> upper motor state
```

These two MIDI targets differ by exactly **one 14-bit raw count**, and the MSB is identical. This is strong evidence that the low byte is processed before a later motor-position conversion or quantization stage.

It is **not** evidence that the motor has 16,384 stable physical positions. The observed behavior at this point is better described as a fine digital target feeding a coarser physical/servo state. The number of stable motor states across the full travel is still unknown.


### 3.3 Reproducible evidence

The two boundary tests included in this repository are:

- [`tools/motor-feedback-lsb-exact-threshold-r5.html`](tools/motor-feedback-lsb-exact-threshold-r5.html) — lower-state scan around `LSB 112..116`;
- [`tools/motor-feedback-lsb-upper-boundary-r8.html`](tools/motor-feedback-lsb-upper-boundary-r8.html) — same-MSB upper-state return scan from `LSB 127` down through the `113/112` boundary.

Raw logs are included as:

- [`captures/motor-feedback-lsb-r5-exact-threshold-report.txt`](captures/motor-feedback-lsb-r5-exact-threshold-report.txt);
- [`captures/motor-feedback-lsb-r8-upper-boundary-report.txt`](captures/motor-feedback-lsb-r8-upper-boundary-report.txt).

A fuller discussion of the methodology and the distinction between **message resolution**, **target resolution** and **physical motor resolution** is in [`docs/motor-feedback-boundary.md`](docs/motor-feedback-boundary.md).

### 3.4 Multi-region anchored validation

A first multi-region pass (R9) showed that using only the local same-MSB LOW/HIGH pair was not always enough to force the motor into a known starting state. R10 therefore used a deliberately distant hard anchor before each local test, while keeping the decisive `112/113` comparison inside the same MSB bucket.

With firmware 1.14, Mackie Control mode and Fader 1, the anchored test reproduced the same result at **MSB 90, 99, 101 and 110**:

```text
from below:  LSB 112 -> lower state
             LSB 113 -> next state

from above:  LSB 113 -> upper state
             LSB 112 -> previous state
```

This made a position-dependent mechanical explanation less likely and gave us a specific digital boundary to look for in the firmware.

### 3.5 Firmware receive routine explains the 112/113 boundary

Static disassembly of the exact firmware image identified by SHA-256
`7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6`
shows the relevant Mackie Pitch Bend receive path around `0x0800BA42`.

The decisive instructions are:

```asm
0x0800BA68  mov   r0, r4        ; target = MSB
0x0800BA6A  cmp   r5, #112      ; r5 = LSB
0x0800BA6C  bls   keep_target
0x0800BA6E  cmp   r0, #127
0x0800BA70  bhs   keep_target
0x0800BA72  adds  r0, r4, #1    ; LSB > 112 -> next 7-bit target
```

So the observed boundary is not merely a servo artifact. For the normal non-saturated range, the receive-side behavior is equivalent to rounding the 14-bit target to a 7-bit motor target with a threshold between LSB 112 and 113.

This is equivalent in result to:

```text
motor7 = (raw14 + 15) >> 7
```

but the firmware implements the threshold explicitly rather than with that literal arithmetic sequence.

The receive-side finding complements the already identified transmit-side call site around `0x080099AC`, where the normal Mackie fader message deliberately sets the low data byte to zero.

Full notes are in [`docs/firmware-rx-tx-path.md`](docs/firmware-rx-tx-path.md).

---

## 4. The motor fader itself is not a convincing 7-bit bottleneck

Behringer lists the **MF100T** as the replacement motor fader for both the X-Touch and X-Touch Compact. It uses a 100 mm, 10 kΩ linear resistive track.

A resistive fader is an analog position source. It does not inherently output a 7-bit number. Resolution is introduced later by the acquisition electronics and firmware.

That did not prove the Compact had a high-resolution acquisition path, but it made the physical fader itself a poor explanation for an exact `0, 128, 256, ...` digital pattern.

Official reference:

- Behringer MF100T: https://www.behringer.com/en/products/0701-ABV

---

## 5. The firmware was already inside X-TOUCH Editor

The macOS X-TOUCH Editor application bundle we inspected contains:

```text
X_TOUCH.app/
  Contents/
    Resources/
      UpdateFiles/
        Xtouch_Compact.bin
        Xtouch_Mini.bin
```

For the `Xtouch_Compact.bin` image in the inspected editor bundle:

```text
size:   52,924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
```

We intentionally do **not** redistribute that binary here.

The image starts with a Cortex-M-style vector table. The first words are:

```text
initial stack pointer: 0x200049B8
reset vector:          0x0800647D
```

A subsequent load-base check corrected an important detail in the initial static analysis: **the firmware file is an application image loaded at `0x08006000`, not at `0x08000000`**.

The evidence is internally consistent:

- startup code at `0x080063E8..0x080063EC` explicitly writes `0x08006000` to the Cortex-M VTOR register at `0xE000ED08`;
- file offset `0x047C` contains the reset/startup stub; with a `0x08006000` image base, that instruction is at `0x0800647C`, exactly matching the reset vector (Thumb bit set);
- its literal targets `0x080063B3` and `0x080060ED` map back inside the same file;
- all flash-like addresses in the vector table are at or above `0x08006000`.

This means the lower `0x6000` bytes of MCU flash are **not contained in `Xtouch_Compact.bin`**. The strongest current interpretation is a separate updater/bootloader region below the application image. Static analysis of X-TOUCH Editor independently shows a boot/update protocol distinct from the normal application protocol. See [`docs/bootloader-updater-notes.md`](docs/bootloader-updater-notes.md).

A safe CoreMIDI virtual-device capture has now clarified several Editor commands without connecting the physical Compact:

```text
@ABQ    normal APP identity query
@AB6    uBoot state query
@ABR    normal hardware Layer A/B retrieval (not a boot transition)
@AB`    later update-workflow command; exact semantics not yet assigned
@ABa    later update-workflow command; exact semantics not yet assigned
@ABb    adjacent Editor command template; exact semantics not yet assigned
```

The Editor's `@AB6` response parser reconstructs a 32-bit value from eight low nibbles and recognizes `0x11112222` as the positive uBoot-state signature. In parser order, those eight nibbles are `02 02 02 02 01 01 01 01`.

This corrects an earlier working hypothesis: the repeated `@AB6` traffic observed after APP identification is uBoot polling, while `@ABR 01` belongs to the ordinary Layer A retrieval path. The next safe step is a stateful virtual-uBoot emulator that answers `@AB6` correctly and records the first post-uBoot command without acknowledging flash operations.


The binary also contains the peripheral addresses expected from the STM32F1 family, including references consistent with ADC, DMA, RCC and GPIO blocks. The STM32F1 family uses a 12-bit ADC.

Official STM32F1 documentation:

- https://www.st.com/en/microcontrollers-microprocessors/stm32f1-series/documentation.html

At this stage we can identify the MCU family from the firmware's memory/peripheral map, but we are **not yet claiming an exact STM32 part number** without a board-level chip marking or an independent device-ID readout.

---

## 6. Nine 16-bit fader samples are present before MIDI quantization

Static disassembly reveals a particularly useful data path.

A DMA-related routine at approximately `0x08006562` copies **nine halfword (16-bit) values** in a loop:

```asm
ldrh.w  r1, [r2, r0, lsl #1]
strh.w  r1, [r3, r0, lsl #1]
strh.w  r1, [r4, r0, lsl #1]
...
cmp     r0, #9
blo     loop
```

A tenth halfword is then handled separately.

Later, the fader processing routine around `0x080099E8` also loops exactly nine times and passes each **16-bit sample** into the fader processing function around `0x0800981E`.

Nine is exactly the number of motor faders on the Compact: eight channel faders plus the master fader.

This alone does not tell us the effective number of noise-free bits, but it shows that the fader path is not born as a 7-bit MIDI value.

---

## 7. The firmware keeps a finer internal position than it transmits

The routine around `0x0800981E` maintains a rolling set of 16 halfword samples and a running sum. Once the 16-sample window is populated, the firmware derives two differently scaled values from that sum:

```asm
ubfx    r0,  r1, #8, #16
ubfx    r10, r1, #4, #16
```

Conceptually:

```text
coarser value ~= sum >> 8
finer value   ~= sum >> 4
```

Because the window contains 16 samples, `sum >> 4` corresponds naturally to a 16-sample average while retaining substantially more position detail than the later 7-bit MIDI output.

The finer value is used in the surrounding decision/hysteresis logic. In other words, the firmware itself has access to finer-grained fader information before it decides what to transmit.

This is the key observation that moved the investigation from *"maybe the hardware is only 7-bit"* to *"the coarse MIDI output is introduced later in the processing path"*.

---

## 8. The Mackie Control routine explicitly zeros the low byte

The most direct evidence appears in the Mackie Control output path around `0x080099A8`.

For an ordinary fader value, the code reduces the value to 7 bits and constructs a message with:

```asm
and     r3, r3, #0x7f
movs    r2, #0x00
movs    r1, #0xe0
...
bl      midi_send
```

Interpreted as the MIDI message assembled at that call site:

```text
status/base = 0xE0   -> Pitch Bend
low byte    = 0x00
high byte   = fader_value & 0x7F
```

That maps directly to what we measured on the wire:

```text
raw14 = 0 + (fader7 << 7)
```

There is also a special branch for the maximum fader value that sets both data bytes to `0x7F`, which explains why the absolute top endpoint can appear as:

```text
LSB=127, MSB=127 -> raw=16383
```

while the rest of the travel remains effectively MSB-only.

### This is the central finding

The measured 7-bit behavior is not merely an accident of Ableton, MIDI decoding, or the Pitch Bend format. The firmware call site constructs the normal Mackie fader message with a zero low data byte.

---

## 9. What this proves — and what it does not

### Supported by the current evidence

1. The Compact's Mackie fader stream is effectively 7-bit over normal travel.
2. The behavior exists outside Ableton Live.
3. The same measurement system sees active low bits from an X-Touch One.
4. The Compact firmware processes nine 16-bit fader samples before MIDI transmission.
5. It maintains a finer internal value during filtering/decision logic.
6. The Mackie output call site explicitly sends `LSB=0` for ordinary fader positions and a 7-bit value as the upper data byte.
7. The Mackie motor-feedback receive path explicitly compares the incoming LSB with `112` and rounds the MSB-derived motor target up only for `LSB > 112`.
8. Anchored tests at MSB 90, 99, 101 and 110 reproduce that same 112/113 boundary.

### Not yet proved

1. The exact usable physical resolution of the MF100T in the Compact chassis.
2. The effective number of noise-free ADC bits after power-supply noise, track noise and mechanical repeatability.
3. Whether a 12-bit-to-14-bit output mapping will feel stable without additional filtering/hysteresis.
4. How many distinct, stable motor positions exist across the full travel outside the four tested MSB regions.
5. The exact MCU part number.
6. A safe, repeatable recovery procedure for experimental firmware on every hardware revision.
7. Compatibility of a future patch with every Compact firmware/board revision.

Those questions matter. A 12-bit ADC does not automatically mean 12 bits of *useful* physical fader resolution.

---

## 10. What a higher-resolution firmware experiment would need to test

The obvious experiment is **not** to invent missing values in the DAW.

It is to preserve the Compact's existing acquisition, calibration, filtering and motor logic, then change only the final representation sent to the host.

A first-principles target would look conceptually like:

```text
analog fader
    -> original ADC acquisition
    -> original filtering/calibration
    -> higher-resolution normalized position
    -> 14-bit Pitch Bend encoding
         LSB = raw14 & 0x7F
         MSB = (raw14 >> 7) & 0x7F
```

If the internal useful position is approximately 12-bit, one natural mapping into MIDI Pitch Bend would be:

```text
raw14 = raw12 << 2
```

But that is only a starting hypothesis. The real patch must preserve:

- endpoint calibration;
- deadband/hysteresis;
- touch behavior;
- motor feedback stability;
- host feedback handling;
- mode switching;
- safe boot/recovery behavior.

Until the bootloader/update protocol and recovery path are fully understood, **flashing experimental firmware is premature**.

---

## 11. Origin of the investigation: SSL Remote

This investigation started from a practical requirement in **SSL Remote**: obtain predictable, fine-grained motorized gain control without guessing where resolution was being lost.

**SSL Remote** is the project in which these measurements originated: multi-channel SSL-style control inside Ableton Live, with dedicated views, hardware focus, motorized fader feedback and controller-specific integration.

The X-Touch One already demonstrated that higher-resolution fader input materially improves gain control. The Compact would be especially interesting if its nine motorized faders could expose the finer position information the firmware already processes internally.

This repository will remain focused on **reproducible X-Touch Compact measurements and analysis**. SSL Remote development and releases remain a separate project.

> If you own an X-Touch Compact, especially a different hardware revision or firmware version, useful contributions are raw MIDI captures, board photos, firmware/editor versions and reproducible test results.

---

## Reproduction and evidence

The evidence chain, addresses and capture methodology are documented in [`docs/evidence.md`](docs/evidence.md).

We recommend reproducing the MIDI measurements before drawing conclusions from any firmware patch.

---

## Firmware redistribution

This repository does not contain the Behringer firmware image. The hash above is provided only to identify the exact image used in this analysis.

If future tooling patches the firmware, the preferred distribution model is a patching tool that operates on a firmware image obtained by the user from their own legitimate X-TOUCH Editor installation, rather than redistributing Behringer's binary.

This is a technical/research project, not legal advice. Anyone publishing or distributing modified firmware should independently evaluate the applicable licence, warranty and interoperability rules in their jurisdiction.

---

## Sources

- Behringer X-Touch Compact: https://www.behringer.com/en/products/0808-AAE
- Behringer MF100T motor fader: https://www.behringer.com/en/products/0701-ABV
- X-Touch Compact Quick Start Guide: https://mediadl.musictribe.com/media/PLM/data/docs/P0B3L/QSG_BE_0808-AAE_X-TOUCH%20COMPACT_WW.pdf
- STMicroelectronics STM32F1 documentation: https://www.st.com/en/microcontrollers-microprocessors/stm32f1-series/documentation.html

---

## Project status

### Confirmed

- [x] Compact Mackie fader TX is effectively MSB-only over normal travel; ordinary Pitch Bend LSB is forced to zero.
- [x] X-Touch One A/B capture demonstrates active low-bit fader transmission through the same measurement chain.
- [x] Nine-channel 16-bit acquisition path and finer internal fader position identified in firmware.
- [x] Compact motor RX uses the Pitch Bend LSB and implements the observed 112/113 rounding boundary.
- [x] Application image load base is `0x08006000`; the lower `0x6000` bytes are absent from the distributed APP binary.
- [x] `@AB6` is the uBoot-state query. Positive signature is nibble-coded `0x11112222`.
- [x] `@ABR` is normal Layer A/B retrieval, not APP-to-uBoot transition.
- [x] Updater preamble observed as `@AB8`, `@AB3`, `@AB4`, `@AB5` after positive uBoot detection.
- [x] Firmware transfer consists of 29 wire frames; frames 1..28 map to the padded `0xE000` APP image.
- [x] Editor transfer CRC algorithm identified: STM32-style CRC32, polynomial `0x04C11DB7`, seed `0xFFFFFFFF`, little-endian 32-bit words.
- [x] Editor pads the APP image to `0xE000`, computes its CRC with offset `0x34` zero, then writes the resulting CRC at `0x34`.
- [x] Original APP global CRC is `0x1C044CDE`; prepared block-0/app-frame CRC is `0x73C44D52`, matching the captured real Editor transfer.
- [x] Vendor Editor 1.21 updater worker contains a reproducible 3-byte stack-canary overwrite in both ARM64 and x86_64 slices.
- [x] Full 29-frame virtual transfer completed; no additional host-side finalize SysEx follows the ACK for frame 28.
- [x] Software MCU reset returns directly to APP; no transient uBoot response was detected in 240 rapid `@AB6` probes over ~1.2 s.
- [x] APP-version gating rejected: a virtual APP reporting firmware 1.13 still leaves Update disabled when `@AB6` is silent.
- [x] Tested Compact power-on combinations `MC + Layer A`, `MC + Layer B`, and `MC + Layer A + Layer B` all return normal APP 1.14.
- [x] TX11 experimental patch candidate prepared offline: preserve coarse MSB, derive LSB from four fine position bits, yielding 16 substeps per coarse step.
- [x] TX11 candidate diff, hashes, endpoint behavior and all 524,288 patch-site input cases validated offline.
- [x] TX11 transfer-image preparation validated against the Editor algorithm.
- [x] Offline validity-predicate analysis rejects simple whole-image CRC-residue rules and supports a saved-marker/recompute comparison model.

### Strong evidence / current model

- The updater/bootloader resides outside the distributed APP image, below `0x08006000`.
- Offset `0x34` is boot/application validity metadata stored in a reserved Cortex-M vector-table word.
- A plausible boot check is: validate APP vector structure; save the word at `0x34`; treat that word as zero; recompute CRC across the `0xE000` APP slot; compare the result with the saved word.
- A Compact whose APP is not considered bootable can remain in the separate updater state; however a Compact-specific, intentional boot-entry/recovery method has not yet been reproduced on the test hardware.

### Still open

- [ ] Directly confirm the real bootloader's APP-validity predicate (bootloader region has not been dumped).
- [ ] Identify a reproducible Compact-specific uBoot/recovery entry method independent of a working APP.
- [ ] Verify bootloader physical flash destination behavior, especially treatment of transfer frame 0.
- [ ] Perform the first physical TX11 flash only after the recovery path is sufficiently characterized.
- [ ] Validate high-resolution TX11 output, motor behavior and SSL Remote end-to-end on hardware.

Latest research notes: see [`docs/bootloader-updater-notes.md`](docs/bootloader-updater-notes.md).

### A note on terminology

MIDI Pitch Bend is a 14-bit **message format**. That does not guarantee that a controller supplies 14 bits of real source resolution. The X-Touch Compact is a useful example of why transport width and effective control resolution must be measured separately.