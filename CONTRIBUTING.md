# Contributing test results

The most useful contributions are **reproducible measurements**, not conclusions.

If you have an X-Touch Compact and want to compare behavior, please include:

- controller model and visible firmware version;
- hardware/board revision if known;
- operating system;
- mode: Standard or Mackie Control;
- host state: standalone capture, DAW open, or control-surface session active;
- capture method/tool;
- which fader was moved and how;
- a short raw MIDI excerpt showing `status`, `LSB`, `MSB` and decoded value.

For board or firmware observations, include the exact source version or hash whenever possible.

Please separate:

- **measured fact** — directly observed in a capture or on hardware;
- **static observation** — directly visible in the inspected binary/disassembly;
- **inference** — a technical interpretation that still needs independent confirmation.

Do not upload vendor firmware binaries to this repository.


## Motor-feedback test contributions

For host-to-motor resolution tests, please also include:

- whether Live was closed or a Control Surface was active;
- the exact raw Pitch Bend values sent;
- whether the MSB was held constant while the LSB changed;
- whether the fader was touched during the test;
- a short video or repeatable physical measurement if claiming sub-MSB motor movement.

Please do not describe a motor movement as high-resolution solely because the transmitted MIDI value was 14-bit; the physical position must be independently observable or measurable.
