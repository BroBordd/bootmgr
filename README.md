# boot-menu

A bare-metal boot menu for rooted Android devices, built on [Stratum](https://github.com/BroBordd/stratum). Runs before the Android framework starts, giving you full control over the boot process.

![Boot Menu](https://github.com/user-attachments/assets/07c84df3-df0b-45ab-9b35-8c32846863a0)

## Features

- Continue to system, reboot, recovery, download mode, or power off
- Confirms all destructive actions before executing
- Tracks Android boot status in real time — animated progress bar while booting, solid green when ready
- Touch and hardware key navigation (volume up/down to navigate, power to confirm)
- Launches extra utility apps from the Advanced menu
- Auto-continues to system after a configurable timeout
- Dry-run mode for testing without executing commands

## Installation

Flash the zip via KernelSU or compatible root manager. The module installs to `/data/adb/modules/boot-menu/` and hooks into early boot via `post-fs-data.sh`. Check the release filename for your target device.

Extras drop into `/data/adb/modules/boot-menu/extras/` — any [Stratum](https://github.com/BroBordd/stratum)-based binary placed there will appear in the Advanced menu. Extras from [stratum-apps](https://github.com/BroBordd/stratum-apps) are included in the release zip by default.

## Building

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/BroBordd/boot-menu
cd boot-menu
```

Or if already cloned:

```bash
git submodule update --init --recursive
```

### 2. Add your device to Stratum

Your device must have a folder under `stratum/devices/<model>/` with `StratumConfig.h`. See [Stratum](https://github.com/BroBordd/stratum) for details.

### 3. Build

```bash
bash scripts/build.sh <device>       # build everything + zip
bash scripts/build.sh <device> -m    # boot menu only
bash scripts/build.sh <device> -e    # extras only
```

Output zip: `stratum/devices/<device>/out/<device>-boot-menu.zip`

### 4. Test without flashing

```bash
bash scripts/run.sh <device>
```

## License

Copyright (C) 2026 BrotherBoard — GNU General Public License v3.0. See [LICENSE](LICENSE).
