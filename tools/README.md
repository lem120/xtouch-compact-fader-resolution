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
