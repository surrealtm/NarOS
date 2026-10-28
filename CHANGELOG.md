# CHANGELOG

## v0.3.0 - unreleased

- Added support for arguments for commands [!38](https://github.com/surrealtm/NarOS/pull/38)

## v0.2.0 - 27.10.2026

- Added PCI and UHCI protocols for device discovery and control [!28](https://github.com/surrealtm/NarOS/pull/28)
- Added XHCI protocol for modern USB drive control [!31](https://github.com/surrealtm/NarOS/pull/31)
- Added HCI abstraction layer for implementations [!30](https://github.com/surrealtm/NarOS/pull/30)
- Added USB and SCSI protocols for mass storage device interaction [!29](https://github.com/surrealtm/NarOS/pull/29)
- Added Block Device drivers for mass storage transfer [!35](https://github.com/surrealtm/NarOS/pull/35)
- Added keyboard event parsing through USB [!34](https://github.com/surrealtm/NarOS/pull/34)
- Added formatted string printing in `os_output_print` [!32](https://github.com/surrealtm/NarOS/pull/32)

## v0.1.0 - 30.09.2026

- Added boot loader that calls into custom kernel implementation [!1](https://github.com/surrealtm/NarOS/pull/1)
- Added a simple VGA video mode display API [!2](https://github.com/surrealtm/NarOS/pull/2)
- Added a control API for time keeping and power management [!7](https://github.com/surrealtm/NarOS/pull/7)
- Added an input API for keyboard events [!12](https://github.com/surrealtm/NarOS/pull/12)
- Added a builtin terminal application [!20](https://github.com/surrealtm/NarOS/pull/20) including simple command dispatching [!22](https://github.com/surrealtm/NarOS/pulls/22)
- Added a string structure [!21](https://github.com/surrealtm/NarOS/pull/21)
