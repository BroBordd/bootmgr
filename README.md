# bootmgr

A hacky boot manager for rooted Android devices, built on top of [Stratum](https://github.com/BroBordd/stratum). Runs before the Android framework starts, giving you full control over the boot process right from power-on.

## Features

- Boot to system, reboot, recovery, download mode, or power off
- Confirms destructive actions before executing
- Tracks Android boot status in real time — animated progress bar while booting, solid green when ready
- Touch and hardware key navigation (volume up/down to navigate, power to confirm)
- Advanced menu for launching extra utility apps
- Auto-continues to system after a configurable timeout
- Dry-run mode for safe testing without executing real commands

## Requirements

- KernelSU (or compatible root manager)
- A device with a [Stratum](https://github.com/BroBordd/stratum) config (`stratum/devices/<model>/StratumConfig.h`)

## Installation

Flash the zip via KernelSU or any compatible root manager. The module installs to:

```
/data/adb/modules/boot-menu/
```

It hooks into early boot via `post-fs-data.sh`. Check the release filename for your target device.

### Extras

Any [Stratum](https://github.com/BroBordd/stratum)-based binary placed in:

```
/data/adb/modules/boot-menu/extras/
```

will appear in the Advanced menu. Extras from [stratum-apps](https://github.com/BroBordd/stratum-apps) are bundled in the release zip by default.

## Building

### 1. Clone with submodules

```sh
git clone --recurse-submodules https://github.com/BroBordd/bootmgr
cd bootmgr
```

Or if you already cloned without submodules:

```sh
git submodule update --init --recursive
```

### 2. Add your device to Stratum

Your device needs a folder under `stratum/devices/<model>/` containing `StratumConfig.h`. See the [Stratum repo](https://github.com/BroBordd/stratum) for details on what that file should contain.

### 3. Build

```sh
bash scripts/build.sh <device>      # build everything and produce a flashable zip
bash scripts/build.sh <device> -m   # boot menu binary only
bash scripts/build.sh <device> -e   # extras only
```

Output zip will be at:

```
stratum/devices/<device>/out/<device>-boot-menu.zip
```

### 4. Test without flashing

```sh
bash scripts/run.sh <device>
```

This uses dry-run mode to simulate the boot menu on your device without actually executing commands.

## Repository Layout

```
bootmgr/
├── docs/        # documentation and assets
├── module/      # KernelSU module files (post-fs-data.sh, module.prop, etc.)
├── scripts/     # build.sh, run.sh, and other helpers
├── src/         # boot menu source code
└── stratum/     # Stratum submodule
```

## License

Copyright (C) 2026 BrotherBoard — licensed under the [GNU General Public License v3.0](LICENSE).
