#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

echo "============================================================"
echo " X-TOUCH COMPACT EDITOR PROTOCOL EMULATOR R4"
echo "============================================================"
echo "SAFETY REQUIREMENT: physical X-Touch Compact must be OFF"
echo "and USB DISCONNECTED before continuing."
echo "This emulator never sends MIDI to a physical device."
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: R4 requires macOS CoreMIDI."
  exit 1
fi

if ! command -v clang >/dev/null 2>&1; then
  echo "ERROR: clang not found. Install Xcode / Command Line Tools."
  exit 1
fi

SRC="xtouch_compact_editor_protocol_emulator_r4.c"
BIN="./xtouch_compact_editor_protocol_emulator_r4"

if [[ ! -x "$BIN" || "$SRC" -nt "$BIN" ]]; then
  clang -std=c11 -O2 -Wall -Wextra "$SRC" -framework CoreMIDI -framework CoreFoundation -o "$BIN"
fi

rm -f XTouch_Compact_Editor_Protocol_Emulator_R4_Report.txt
exec "$BIN"
