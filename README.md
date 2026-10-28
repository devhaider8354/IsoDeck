# IsoDeck

[![Build](https://github.com/devhaider8354/IsoDeck/actions/workflows/build.yml/badge.svg)](https://github.com/devhaider8354/IsoDeck/actions/workflows/build.yml)

**English** | [العربية](README.ar.md)

IsoDeck turns a USB drive into a multiboot drive: copy as many Windows, Linux and rescue-tool ISO files as
you like onto it and pick one from a themed boot menu. Single-file native Windows app
(C++17 / Win32 / Direct2D), Arabic and English UI.

## Features

- **One click to prepare a drive** – installs the boot engine (UEFI + Legacy BIOS, optional Secure Boot support, GPT/MBR, exFAT).
- **ISO manager** – add, copy and remove images with progress; no reformatting when you add more.
- **Branded boot menu** – custom background, accent colour, title and timeout; friendly names for each image.
- **Windows 11 setup tweaks** – skip TPM / Secure Boot / RAM checks and the online-account requirement.
- **Unattended installs** – attach `autounattend.xml` templates to Windows images.
- **Download catalog** – Windows, Linux and rescue tools with SHA-256 verification (editable `catalog.txt`).
- **Safe by design** – only USB disks are listed and the Windows system disk can never be selected.
- **Arabic / English UI** with right-to-left layout, dark modern design, per-monitor DPI aware.
- No runtime dependencies: one statically linked `IsoDeck.exe`.

## Download

Get `IsoDeck-Setup.exe` from the [Releases](../../releases) page (Windows 10/11, x64, administrator rights required).

## How it works

The boot engine is the open-source [Ventoy](https://www.ventoy.net) project (GPLv3). The official
Windows package is embedded inside `IsoDeck.exe`, extracted on first use to
`%LOCALAPPDATA%\IsoDeck\engine\`, and driven through its command-line interface:

```
Ventoy2Disk.exe VTOYCLI /I|/U /PhyDrive:N [/GPT] [/NOSB] [/FS:exfat] [/R:MB]
```

Progress and results are read from `cli_percent.txt` / `cli_done.txt`. IsoDeck then writes the
menu configuration (`ventoy/ventoy.json`) and a GRUB theme (`ventoy/theme/isodeck/`) to the drive.

## Build

Requirements: Visual Studio 2022 or newer with *Desktop development with C++*, CMake 3.20+.

1. Download the official Ventoy Windows package `ventoy-x.y.z-windows.zip` from
   https://github.com/ventoy/Ventoy/releases, verify its SHA-256, and save it as **`third_party/ventoy.zip`**.
2. Run `build.bat` → `build\Release\IsoDeck.exe` (and `dist\IsoDeck-Setup.exe` if Inno Setup 6 is installed).

Without `third_party/ventoy.zip` the app still builds, but *Prepare drive* is disabled.

Manual build:

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

The GitHub Actions workflow downloads and verifies the latest Ventoy release automatically.

## Project layout

```
src/main.cpp      entry point, admin check, single instance
src/app.*         window (custom frame), pages: Devices / ISO files / Theme / Catalog / Settings
src/ui.*          immediate-mode widgets (RTL aware, animated)
src/gfx.*         Direct2D + DirectWrite wrapper, vector icons, bitmaps
src/disk.*        physical disk enumeration (USB filter, system-disk guard, Ventoy fingerprint)
src/ventoy.*      embedded package extraction + Ventoy2Disk CLI install/update with progress
src/jobs.*        background queue: copy (CopyFileEx), download (WinHTTP), SHA-256 (BCrypt)
src/theme.*       GRUB theme.txt + ventoy.json generator (aliases, auto install, timeout, Win11 bypass)
src/i18n.*        Arabic / English strings
res/              icon, manifest (requireAdministrator, PerMonitorV2 DPI), default background, catalog
installer/        Inno Setup script
```

## Runtime files

| Path | Purpose |
|------|---------|
| `%LOCALAPPDATA%\IsoDeck\config.txt` | settings, theme, aliases |
| `%LOCALAPPDATA%\IsoDeck\catalog.txt` | editable download catalog (`Category \| Name \| URL \| Note \| direct/page`) |
| `%LOCALAPPDATA%\IsoDeck\engine\` | extracted boot engine (`cli_log.txt` = last install log) |
| `%LOCALAPPDATA%\IsoDeck\isodeck.log` | app log |
| `<USB>\ventoy\ventoy.json` | generated menu configuration |
| `<USB>\ventoy\theme\isodeck\` | generated GRUB theme + background |

## Notes

- GRUB cannot shape Arabic text, so the boot menu uses Latin letters for the title and image names.
- Back up the USB drive first: preparing a drive erases it.

## License

[GPL-3.0](LICENSE) © 2026 FLOW POINT – developed by Haider ([flowp.uk](https://flowp.uk)).
IsoDeck embeds [Ventoy](https://github.com/ventoy/Ventoy) (GPLv3) by longpanda.
