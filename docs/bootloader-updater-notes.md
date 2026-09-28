# Bootloader / updater notes

## Status

This document records static analysis of the updater path in X-TOUCH Editor. It is intended to establish a recoverable firmware-development workflow before any experimental image is flashed.

The current result is encouraging but **does not yet constitute a verified recovery procedure**. A safe virtual-device capture has now separated the normal application query, the uBoot state query, ordinary Editor layer reads, and later update-workflow commands.

## Application image is loaded at 0x08006000

For the analyzed `Xtouch_Compact.bin`:

```text
size:   52,924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
```

Its vector table begins:

```text
SP:            0x200049B8
reset vector:  0x0800647D
```

The load base is also set explicitly by the application startup code:

```asm
0x080063E8  ldr   r1, [pc, ...]   ; 0xE000ED08 (SCB->VTOR)
0x080063EA  ldr   r0, [pc, ...]   ; 0x08006000
0x080063EC  str   r0, [r1]
```

So the application directly programs the Cortex-M vector-table offset register to `0x08006000`.

The reset code is physically present at file offset `0x047C`. With the same load base, that becomes address `0x0800647C`, exactly matching the reset vector after clearing the Thumb bit.

The stub loads targets `0x080063B3` and `0x080060ED`; both map back into the same file under the same base.

All flash-like entries inspected in the vector table are at or above `0x08006000`.

### Current interpretation

`Xtouch_Compact.bin` is an **application image**, not a complete flash dump beginning at `0x08000000`.

The flash range below `0x08006000` is absent from the vendor application image. Combined with the separate update protocol described below, this is strong static evidence for an independent bootloader/updater region occupying some or all of the lower `0x6000` bytes.

We have not dumped that lower region from hardware, so its exact contents and full size remain unverified.

## X-TOUCH Editor creates SysEx frames itself

The Editor helper at macOS ARM64 address `0x100032F94` takes a payload, allocates two extra bytes, prepends `F0`, appends `F7`, and sends the resulting MIDI SysEx message.

This lets the constant updater templates be interpreted directly.

## Normal application identity query

The Editor's normal application identity request uses a 12-byte payload beginning:

```text
40 41 42 51
```

or ASCII `@ABQ`.

On the wire, the request is therefore:

```text
F0 40 41 42 51 00 00 00 00 00 00 00 00 F7
```

The normal application firmware contains a handler for command `0x51`.

## Separate updater / uBoot query

The Editor's update-mode detector uses a 20-byte payload beginning:

```text
40 41 42 36
```

or ASCII `@AB6`.

On the wire:

```text
F0 40 41 42 36
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
F7
```

The Editor waits for a response with matching `40 41 42 36` header and decodes a 32-bit value from eight low nibbles in response bytes 5 through 12.

The normal application parser does not implement the updater command family observed in the Editor (`0x33`, `0x34`, `0x35`, `0x36`, `0x38`). This supports the interpretation that those messages are handled by code outside the application image.

## Safe Editor-protocol emulation (R3)

A CoreMIDI virtual-device emulator was used with the physical X-Touch Compact powered off / disconnected. The emulator answered only the normal APP `@ABQ` query with the real firmware-1.14 identity reply and logged every other SysEx without acknowledging it.

The observed Editor sequence was:

```text
@ABQ       -> APP identity query
@AB6       -> repeated uBoot-state polling
@ABQ       -> APP re-check
@ABR 01    -> ordinary Editor command, retried after about 800 ms
...
@AB` 00
@ABa 01
```

This capture is important because it shows that `@AB6` is **not** an APP-to-uBoot transition command. It is the Editor asking whether uBoot is already present.

### uBoot signature expected by the Editor

Static analysis of the Editor's `@AB6` response parser shows that it reconstructs a 32-bit value from eight low nibbles in response bytes 5 through 12 and tests that value against:

```text
0x11112222
```

When the reconstructed value matches, the Editor marks uBoot as present.

With the parser's nibble order, the eight response nibbles representing that signature are:

```text
02 02 02 02 01 01 01 01
```

This gives the virtual-device work a concrete, testable uBoot-state response rather than an invented acknowledgement value.

## `@ABR` is not a boot transition command

The R3 capture initially made `@ABR 01` look like a possible transition request because it appeared after unsuccessful `@AB6` polling.

Static analysis of the Editor resolves that ambiguity: `@ABR` belongs to the normal hardware-layer read path. The Editor uses variants including `@ABR 01` / `@ABR 02` for Layer A / Layer B retrieval.

Therefore:

```text
0x52 / @ABR != APP -> uBoot transition
```

## Later update-workflow commands

The R3 capture also exposed two later command families:

```text
0x60 -> @AB`
0x61 -> @ABa
```

The Editor contains dedicated send routines for these command templates, and the firmware-update path reaches them after the normal APP/uBoot probing phase.

Their exact semantics are **not yet established**, so they are documented here as update-workflow candidates rather than named as erase, reboot, write, or transition operations.

A further adjacent command template `0x62 -> @ABb` is also present in the Editor binary.

The next safe experiment is therefore a stateful virtual device (R4):

```text
virtual APP 1.14
    -> answer @ABQ
    -> remain silent to @AB6

observed update transition
    -> switch virtual state only

virtual uBoot
    -> stop answering @ABQ
    -> answer @AB6 with the 0x11112222 signature
    -> log the first post-uBoot command without acknowledging flash operations
```

No `@AB\``, `@ABa`, `@ABb`, `@AB3`, `@AB4`, `@AB5` or `@AB8` command needs to be sent to the physical Compact for this experiment.

## Updater block structure

The updater thread:

1. allocates `0xE800` bytes;
2. loads the application file starting at buffer offset `+0x800`;
3. computes an overall CRC across `0xE000` bytes of the application-area buffer;
4. stores that CRC into the image/header area;
5. iterates over **29 blocks** of `0x800` bytes;
6. computes a per-block CRC;
7. packs raw data into a MIDI-safe 7-bit representation;
8. sends each block and waits for an acknowledgement.

Twenty-eight blocks of `0x800` equal exactly `0xE000`.

This strongly suggests an updater layout consisting of one protocol/control block plus a `0xE000` application slot, which is consistent with an application range beginning at `0x08006000` and extending to `0x08013FFF`.

The virtual updater captures R4.3-R4.6 now confirm the transfer structure experimentally.

## Live virtual updater capture: R4.3-R4.6

With the physical Compact disconnected, R4.3 answered `@AB6` with the positive nibble signature `0x11112222`. This directly enabled the Editor's Update path and produced the updater preamble:

```text
@AB8
@AB3
@AB4
@AB5
```

followed by a large non-`@AB` SysEx transfer frame.

R4.4 captured the first complete transfer frame. Its total wire length is 2364 bytes:

```text
F0
00 20 32 00 1E 34 00
[8 CRC nibbles]
[2 block-index nibbles]
00
[2344 bytes of MIDI-safe payload]
F7
```

The 2344-byte payload decodes to one 2048-byte raw block plus the three-byte packing overrun/padding implied by groups of seven raw bytes encoded into groups of eight MIDI-safe bytes.

The packing helper is now verified end-to-end: each group of seven raw bytes becomes eight 7-bit-safe bytes. The first seven output bytes contain the low seven bits of each input byte, and the eighth collects their MSBs.

### CRC algorithm

The Editor CRC helper is equivalent to the STM32 CRC peripheral algorithm:

```text
polynomial: 0x04C11DB7
seed:       0xFFFFFFFF
input:      32-bit little-endian words
xor-out:    none
```

The application file is zero-padded to `0xE000` bytes. Before block transfer, the Editor computes the CRC across that entire padded image and stores the 32-bit result at application offset `0x34`.

For the analyzed vendor image:

```text
overall CRC:        0x1C044CDE
stored little-endian: DE 4C 04 1C
image offset:       0x34..0x37
```

After that patch, the first application block (`0x0000..0x07FF`) has per-block CRC:

```text
0x73C44D52
```

which exactly matches the nibble-coded CRC observed in transfer frame 1.

### Block mapping

R4.6 proved the transfer acknowledgement and block progression. The asynchronous transfer callback expects the same nibble value `0x11112222`, but aligned so the signature begins at raw SysEx bytes 5..12:

```text
F0 40 41 42 36
02 02 02 02 01 01 01 01
00 00 00 00 00 00 00
F7
```

After this acknowledgement for transfer block 0, the Editor emitted transfer block 1 about 5.3 ms later.

Decoding transfer block 1 produces the first `0x800` bytes of `Xtouch_Compact.bin`, with exactly one expected modification: bytes `0x34..0x37` contain the overall CRC `DE 4C 04 1C` inserted by the Editor.

This experimentally confirms the mapping of blocks 1..28 to the padded application image, but a later cross-run comparison corrects the earlier interpretation of block 0.

### Block 0 is uninitialized host-buffer data

The Editor allocates `0xE800` bytes with array `new[]` and does not clear the allocation before loading the application at `buffer + 0x800`. The transfer loop nevertheless begins at block index 0.

Comparing independently captured block-0 frames from R4.4 and R4.6 shows 778 differing raw bytes out of 2048. Their per-block CRCs also differ:

```text
R4.4 block 0 CRC: 0x8EC244F7
R4.6 block 0 CRC: 0x10E2F629
```

This is consistent with stale/uninitialized heap contents, not a deterministic updater control structure.

The corrected host-side mapping is therefore:

```text
transfer block 0  = first 0x800 bytes of the uninitialized 0xE800 host allocation
transfer block 1  = application 0x0000..0x07FF
transfer block 2  = application 0x0800..0x0FFF
...
transfer block 28 = final 0x800-byte block of the zero-padded 0xE000 application slot
```

This does **not** establish what the bootloader does with block index 0. It may ignore it, treat it specially, or map indices independently. The bootloader's physical flash-write address calculation remains unverified.

## Why this matters for recovery

The most important current observation is architectural:

```text
lower flash / updater code     application image
0x08000000 ...                 0x08006000 ...
        separate                    Xtouch_Compact.bin
```

A TX-only patch at application file offset `0x3996` therefore corresponds to physical address:

```text
0x08006000 + 0x3996 = 0x08009996
```

and does not modify the lower updater region if the Editor writes only the application slot as the static analysis indicates.

## Remaining blockers

The remaining high-value questions are now narrower:

1. determine the bootloader's physical destination address calculation for transfer blocks 1..28;
2. identify the exact roles of the `@AB8/@AB3/@AB4/@AB5` preamble commands and whether any represent erase/setup/finalize states;
3. identify the exact semantics of `@AB\``, `@ABa` and `@ABb` in the broader firmware-update workflow;
4. verify a **Compact-specific** boot-entry/recovery method that does not depend on a working application.

Published instructions located so far describe the X-Touch Mini's power-on update gesture, but that button combination must not be assumed to apply to the Compact without evidence.

Until those recovery details are verified, experimental firmware flashing remains on hold.


## Completion of the 29-block host transfer

R4.7 acknowledged all transfer frames with header indices 0 through 28. The Editor emitted no additional SysEx after the acknowledgement for frame 28.

Static analysis matches the capture: the transfer loop increments the block index and terminates when it reaches `0x1D` (29 blocks). The worker then returns success directly; there is no host-side post-transfer SysEx in this routine.

Therefore the earlier expectation of a finalize/reboot SysEx after frame 28 was incorrect.

During the R4.7 virtual run, X-TOUCH Editor crashed after the final acknowledgement. The capture itself had already completed successfully. The crash is therefore a host-side post-loop/teardown event, not evidence of a missing post-transfer MIDI command. Its exact cause remains unassigned pending crash-log or teardown-path analysis.

No further virtual runs should intentionally progress beyond the final block acknowledgement until the host teardown path is understood.


## Host-side teardown after the transfer loop

Static analysis of the worker wrapper shows that after the 29-block transfer routine returns, the Editor does not send further MIDI. The wrapper:

1. returns from the transfer routine;
2. clears a global/state value;
3. frees/clears a host-side buffer object;
4. enters the thread/run-loop notification/cleanup path.

This matches the R4.7 capture, where no post-transfer SysEx was observed after block 28.

The X-TOUCH Editor crash seen after the final virtual acknowledgement therefore occurs in host-side post-loop cleanup or UI/thread notification, not in a missing MIDI finalize command.

## Physical destination mapping: strongest supported inference

The Editor never transmits a flash address in the captured block frames; it transmits only a block index. Therefore the physical destination address is selected entirely by the bootloader and cannot be proven from the host binary alone.

However, the application image is known to begin at `0x08006000`, and 28 blocks of `0x800` bytes exactly cover the padded `0xE000` application slot:

```text
block 1  -> inferred application address 0x08006000
block 2  -> inferred application address 0x08006800
...
block 28 -> inferred application address 0x08013800 .. 0x08013FFF
```

For `i >= 1`, the natural mapping is therefore:

```text
address(i) = 0x08006000 + (i - 1) * 0x800
```

This is a strong architectural inference, not yet a direct bootloader observation.

Block 0 cannot safely be mapped linearly to `0x08005800`: the host sends uninitialized heap contents in block 0, and writing those bytes would corrupt 2 KiB immediately below the application on every official update. Therefore the bootloader must treat block index 0 specially (for example, ignore its payload or use the frame only as a protocol/setup stage). The exact block-0 bootloader behavior remains unverified.


## ARM64 X-TOUCH Editor 1.21.0 stack overflow in updater worker

The R4.7 virtual run completed all 29 transfer frames and then X-TOUCH Editor 1.21.0 aborted on macOS 13.7.8.

The macOS crash report identifies:

- signal: `SIGABRT`
- diagnostic: `stack buffer overflow`
- faulting thread: `Update xTouch firmware thread`
- termination path through `__stack_chk_fail`

Static analysis of the native ARM64 slice explains the failure exactly.

The transfer worker beginning around `0x1000039FC` allocates a large local stack frame:

```asm
mov  w9, #0x1AD0
bl   ___chkstk_darwin
sub  sp, sp, #0x1000
sub  sp, sp, #0xAD0
```

The stack canary is stored at effective offset `sp + 0x1AC8` after the large allocation.

Later the worker constructs the packed transfer body in a destination beginning at `sp + 0x11A0` and performs:

```asm
memcpy(dest, src, 0x92B)
```

The write ends at:

```text
0x11A0 + 0x92B = 0x1ACB
```

which overlaps the stack-canary region at `0x1AC8` by three bytes.

The canary is checked only when the transfer loop reaches block count `0x1D` (29) and exits. At that point the mismatch branches to `__stack_chk_fail`, producing the observed crash.

Therefore the R4.7 crash is **not** evidence of a missing post-transfer MIDI command or teardown race. It is a reproducible host-side ARM64 buffer-overflow defect in X-TOUCH Editor 1.21.0's firmware-update worker.

No physical Compact was connected during this finding.


### The same overflow exists in the x86_64 slice

The x86_64 build of X-TOUCH Editor 1.21.0 contains the same off-by-three stack overwrite.

Relevant layout:

```text
packed-frame destination: rbp - 0x950
memcpy length:             0x92B
write end:                 rbp - 0x25
stack canary:              rbp - 0x28
```

Therefore the copy overwrites three bytes of the stack canary on x86_64 as well. The x86_64 worker then performs the same `__stack_chk_guard` comparison and calls `__stack_chk_fail` on mismatch.

This rules out Rosetta/x86_64 execution as a workaround for the host-side updater crash.


## R4.8-R4.25: transfer completion, boot-entry probes and APP-validity model

The later R4 experiments substantially narrowed the remaining unknowns.

### Transfer completion and post-transfer behavior

Virtual updater runs progressed through all 29 transfer frames. After the acknowledgement for frame 28, the Editor's transfer routine considers the host-side transfer complete. No mandatory bootloader finalize SysEx was found after that final acknowledgement.

Subsequent `@AB6` traffic observed after virtual reboot is best interpreted as device rediscovery/polling, not as a flash-finalization command. Normal APP commands `0x60` and `0x61` are therefore no longer treated as required bootloader-finalize operations.

### Software reset does not expose a transient uBoot window

The normal APP accepts the MCU reset message:

```text
F0 00 00 66 14 08 00 F7
```

A physical R4.20 test confirmed APP 1.14 before reset, then sent exactly one software reset and polled only `@AB6` rapidly for approximately 1.2 seconds.

Result:

```text
240 @AB6 queries
uBoot-positive: NO
final state: APP 1.14
```

This rejects the hypothesis that a short, easily missed uBoot response window appears after an ordinary software reset.

### APP firmware version is not the Editor's Update gate

R4.22 presented the Editor with a virtual normal APP reporting firmware 1.13 while deliberately leaving `@AB6` unanswered.

The Editor correctly identified APP 1.13, continued normal APP/Layer-A interaction, but Update remained disabled.

Therefore:

```text
APP 1.14 + silent @AB6 -> Update disabled
APP 1.13 + silent @AB6 -> Update disabled
positive @AB6 signature -> Update enabled
```

The Editor's Update gate is uBoot state, not simply a newer bundled firmware version.

### Tested Compact power-on combinations

A read-only R4.23 detector observed endpoint naming plus only `@ABQ` and `@AB6` while the physical Compact was power-cycled with selected button combinations.

The following combinations all returned the normal `X-TOUCH COMPACT` endpoint and APP 1.14:

```text
MC + Layer A
MC + Layer B
MC + Layer A + Layer B
```

The previously tested two-leftmost-lower-buttons combination also did not enter uBoot on the Compact test unit.

These results should be treated as negative evidence for those specific gestures only; they do not prove that no hardware boot-entry gesture exists.

### CRC metadata at APP offset 0x34

The Editor's transfer-image preparation is now cross-checked against a real captured transfer frame.

For the original Compact APP:

```text
vendor APP word @0x34:               0x00000000
CRC over padded 0xE000 APP
  with @0x34 treated as zero:        0x1C044CDE
Editor-prepared word @0x34:          0x1C044CDE
CRC of prepared first 0x800 block:   0x73C44D52
```

The first-block CRC exactly matches the real Editor transfer capture.

The corresponding TX11 candidate values are:

```text
CRC inserted at @0x34:               0x83D4E96E
CRC of prepared first 0x800 block:   0xCA825D83
```

Compact and Mini vendor APP images use the same structural convention: APP base `0x08006000`, plausible Cortex-M vectors, and a zero word at `0x34` before Editor transfer preparation.

### R4.25 validity-predicate analysis

Offline predicate testing rules out several simple interpretations of the prepared image:

```text
CRC(prepared whole APP) == 0                 -> false
CRC(prepared whole APP) == stored @0x34      -> false
stored @0x34 == ~CRC(prepared whole APP)     -> false
```

For the original Compact image:

```text
CRC(prepared whole APP) = 0x46E4FC51
```

For TX11:

```text
CRC(prepared whole APP) = 0x9D7ABDB7
```

The model most consistent with all current evidence is therefore:

1. validate basic APP vector structure;
2. read the saved metadata word at APP offset `0x34`;
3. treat that word as zero;
4. recompute the STM32-style CRC across the padded `0xE000` APP area;
5. compare the recomputed CRC with the saved word.

This is **strong evidence**, not a direct observation of the bootloader implementation. The lower bootloader region has not been dumped, so additional GPIO, hardware-revision, timeout, or metadata checks remain possible.

### Current physical-flash blocker

The TX11 patch itself has passed offline diff validation, exhaustive patch-site behavioral testing, endpoint preservation, and Editor transfer-image construction.

The remaining blocker is not TX11 image construction. It is a Compact-specific recovery/uBoot entry method that is reproducible independently of a working APP.

Until that path is demonstrated, intentionally corrupting the APP merely to force the bootloader remains outside the test plan.
