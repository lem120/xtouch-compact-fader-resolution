# Publishing plan

## Suggested repository name

`xtouch-compact-fader-resolution`

## Suggested GitHub description

`Measurements, protocol observations and firmware-level notes on X-Touch Compact motor-fader resolution.`

## Recommended first release

Publish the repository as **research**, not as a firmware release.

1. Put the technical article in `README.md`.
2. Put offsets, disassembly and test methodology in `docs/evidence.md`.
3. Do not upload the vendor firmware binary.
4. Add short raw captures that reproduce the Compact vs X-Touch One result.
5. Add 2–3 annotated screenshots/plots only if they make the evidence easier to understand.
6. Open GitHub Discussions or Issues for hardware revision / firmware-version reports.
7. Link to SSL Remote only in the origin/"why this matters" section and at the end.
8. Keep the Ableton `value_pair_map` observation explicitly labelled as community-reported until independently reproduced.
9. Publish the LSB-only motor test as a reproducible next experiment, not as a claimed result.

## Suggested launch post

**Title:** `X-Touch Compact fader resolution: what the MIDI stream and firmware path show`

**Short copy:**

> While building SSL Remote I hit a repeatable resolution limit on the X-Touch Compact motor faders. I initially suspected Ableton or my MIDI mapping. Raw captures ruled that out. An A/B test with X-Touch One showed active low bits, while the Compact stayed MSB-only. I then found the Compact firmware inside X-TOUCH Editor and traced the fader-related path in the binary. The firmware processes 16-bit samples and retains a finer internal position, but the Mackie Control output call site explicitly sends a zero low data byte for ordinary fader positions. I have published the measurements, offsets and reproduction method here. No custom firmware yet — first priority is a safe recovery path.

Use the repository link as the only CTA. Let the technical work lead readers naturally to SSL Remote.


## Social hook

A more provocative framing can be used on social media without changing the repository title:

> **Does the X-Touch Compact have a hidden 14-bit fader path?**
>
> Raw MIDI says the normal Mackie output behaves like 7-bit. The firmware appears to retain finer fader information internally. We published the measurements and are now testing the motor-feedback direction separately.

Avoid stating that a hidden 14-bit mode already exists. The question is the hook; the repository contains the evidence and open tests.
