# Firmware notes

No Behringer firmware binary is stored in this repository.

The firmware image used for the documented analysis was found inside a local macOS X-TOUCH Editor installation at:

```text
X_TOUCH.app/Contents/Resources/UpdateFiles/Xtouch_Compact.bin
```

Reference identity for that image:

```text
size:   52924 bytes
SHA256: 7d03b5174f4987d618fb2dadfda50ec65be2054bab3d12a158db12cbdc7941c6
```

Before comparing offsets or applying any future patch, verify that your image has the same hash. Different editor or firmware releases may use different layouts.

Do not flash experimental firmware until a tested recovery path is available for your hardware revision.


## Later development and download status

The stock identity above is the original analysis reference, not the later custom baseline. The consolidated TEST03c image and its internal v1.0.0 alias are identified separately in [current development status](../docs/current-development-status.md).

A free download is planned. See [DOWNLOADS.md](../DOWNLOADS.md) for availability, package requirements and the distinction between firmware and the receiving driver. No firmware image is added by this documentation update.

