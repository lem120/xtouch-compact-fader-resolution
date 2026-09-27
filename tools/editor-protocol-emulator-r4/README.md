# Editor protocol emulator R4

R4 is a safe CoreMIDI virtual-device experiment for the X-Touch Compact firmware-update workflow.

The physical X-Touch Compact must remain powered off and USB-disconnected.

## Known protocol roles

- `@ABQ / 0x51`: normal APP identity query.
- `@AB6 / 0x36`: uBoot state query.
- `@ABR / 0x52`: normal Layer A/B retrieval, not a boot transition.
- Positive uBoot signature expected by the Editor: `0x11112222`, represented by low nibbles `02 02 02 02 01 01 01 01`.

## R4 transition heuristic

R3 observed:

```text
@AB` 00
@ABa 01
```

about 4 ms apart. R4 treats only this exact pair, in this order within 250 ms, as a **virtual** APP -> UBOOT transition marker.

It does not assign erase/write/reboot semantics to either command individually.

## Safety

R4 replies only to APP `@ABQ` and, after the virtual state transition, uBoot `@AB6`.

It never acknowledges `@AB3`, `@AB4`, `@AB5`, `@AB8` or any other post-uBoot update command. The first post-uBoot `@AB...` command is logged and the emulator stops.

## Running

1. Disconnect and power off the physical Compact.
2. Close Ableton and X-TOUCH Editor.
3. Run `start_emulator_r4.command`.
4. Open X-TOUCH Editor and enter the normal firmware-update workflow.
5. Use the original Behringer `Xtouch_Compact.bin` if the Editor requests a file.
6. Return `XTouch_Compact_Editor_Protocol_Emulator_R4_Report.txt`.

This is a protocol-emulation experiment, not a firmware flasher.
