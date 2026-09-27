# Tools

Small, non-destructive utilities used to reproduce specific measurements in this repository.

## motor-feedback-lsb-test.html

Web MIDI test for the **host -> motor** direction. It sends ordinary Pitch Bend fader-position messages while holding the MSB constant and varying only the LSB.

It does not enter firmware-update mode and does not write flash. Close DAWs and other MIDI applications before running it so that they do not send competing motor feedback.

Recommended test:

1. Put X-Touch Compact in Mackie Control mode.
2. Open the HTML file in Chrome and grant MIDI access.
3. Select `X-TOUCH COMPACT` as output.
4. Do not touch the fader while the motor test is running.
5. Use Fader 1 / channel 1 first.
6. Send the fixed-MSB sequence around raw 12800.
7. Observe whether the motor takes distinct positions while MSB remains 100.

Record the exact controller firmware version and preferably video the fader against a fixed reference.

## motor-feedback-lsb-test-r3.html

Follow-up forced-hold test for the same **host -> motor** path. R3 repeatedly retransmits each target at 20 Hz, always returns to the same raw `12800` baseline, and then tests fixed-MSB LSB offsets individually. This is intended to separate true sub-step positioning from servo settling, deadband and snap-back behavior observed during the first staircase test.

Run the SHORT map first and video the tested fader against a fixed reference.


## R5 / R8 same-MSB boundary tests

### `motor-feedback-lsb-exact-threshold-r5.html`

Tests the lower-state threshold around `LSB 112..116` while holding `MSB=100`.

### `motor-feedback-lsb-upper-boundary-r8.html`

Arms the fader at `LSB=127, MSB=100`, then walks downward through `127..112` without changing the MSB.

### Current observed boundary

At the tested operating point on firmware 1.14:

```text
12912 = LSB 112 / MSB 100 -> lower state
12913 = LSB 113 / MSB 100 -> upper state
```

See `docs/motor-feedback-boundary.md` for interpretation and limitations.


## R9 / R10 multi-region validation

### `motor-feedback-lsb-multi-region-r9.html`

First multi-region 112/113 test at MSB 90, 99, 101 and 110. This test exposed a reset-state confound: the local same-MSB span was not always enough to guarantee a known starting state.

### `motor-feedback-lsb-anchored-hysteresis-r10.html`

Corrected version. Each trial first uses a distant hard anchor to establish direction/state, then settles on the local LOW or HIGH reference and performs the decisive 112/113 comparison inside the same MSB bucket.

R10 reproduced the same 112/113 boundary at all four tested regions. The firmware receive routine later explained that exact threshold.
