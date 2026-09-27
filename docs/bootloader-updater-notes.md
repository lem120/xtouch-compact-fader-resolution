# Bootloader / updater notes

## Status

This document records static analysis of the updater path in X-TOUCH Editor. It is intended to establish a recoverable firmware-development workflow before any experimental image is flashed.

The current result is encouraging but **does not yet constitute a verified recovery procedure**.

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

The reset code is physically present at file offset `0x047C`. With a load base of `0x08006000`, that becomes address `0x0800647C`, exactly matching the vector after clearing the Thumb bit.

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

That mapping remains an **inference from Editor behavior**, not yet a live bus capture of bootloader flash addresses.

## Acknowledgement

The Editor's update-message parser reconstructs a nibble-encoded 32-bit word and sets its block-acknowledgement flag when that word equals:

```text
0x11112222
```

The exact semantic name of that value is not yet established; it should be treated as an observed updater acknowledgement/signature, not as an application firmware identity value.

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

## Remaining blocker

We still need a **verified X-Touch Compact method for entering update mode independently of a working application**.

Published instructions located so far describe the X-Touch Mini's power-on update gesture, but that button combination must not be assumed to apply to the Compact without evidence.

Until the Compact boot-entry/recovery procedure is verified, experimental firmware flashing remains on hold.
