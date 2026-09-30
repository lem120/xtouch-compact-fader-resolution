# TX11 Physical Validation Milestone

## Current milestone

An experimental X-Touch Compact firmware build has now been successfully installed on physical hardware and validated with direct MIDI monitoring in Mackie Control mode.

The key result is that the transmitted Pitch Bend low data byte (LSB), which was effectively fixed during normal travel in the stock behavior, is now demonstrably active on the physical unit.

Observed examples include non-zero LSB values such as:

```text
E0 48 01
E0 58 03
E0 60 05
E0 78 06
E0 10 0D
E0 20 41
```

The full-scale endpoints are also preserved:

```text
minimum: E0 00 00
maximum: E0 7F 7F
```

## What this proves

This is the first end-to-end physical confirmation that the Compact's Mackie Control fader output can be altered at firmware level so that meaningful data is present in the Pitch Bend LSB.

It therefore validates the broader working hypothesis of this investigation: the normal coarse fader stream is not imposed by the MIDI transport itself, and the firmware output path can be changed on real hardware.

## Important limitation

The current experimental build does **not yet provide the intended finer absolute fader-position resolution**.

During slow monotonic sweeps, the LSB tends to remain at a repeated value while the MSB changes by one step at a time. Different LSB values also appear depending on movement direction and dynamics.

That means the current fine-byte source is not yet behaving as the desired absolute sub-position value. The transmitted stream is different from stock and the LSB path is active, but smooth motion still behaves approximately like the original coarse positional resolution.

## Next research target

The next step is to trace the fader-processing path further upstream and identify the correct internal absolute position value **before** the final 7-bit quantization stage.

A successful next revision should demonstrate multiple stable LSB substeps while the MSB remains unchanged, independent of movement direction.

---

This note intentionally records only the current technical milestone and observed result. It does not document firmware installation, boot-entry, recovery, or update procedures, and no firmware binary is distributed here.
