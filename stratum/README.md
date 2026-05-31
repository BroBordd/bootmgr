# Stratum

Native Android overlay framework. Fullscreen OpenGL ES 2.0 surface, raw input, ~60fps render loop. No Activity, no framework, no JVM.

Talks directly to SurfaceFlinger via `SurfaceComposerClient`, creates a surface at `INT_MAX` layer, reads raw input events from `/dev/input/` in a dedicated thread.

---

## Device Support

Stratum is device-specific by design. Each supported device lives under `devices/`:

```
devices/
└── <model>/
    ├── StratumConfig.h   ← device paths and touch parameters
    ├── app.cpp           ← your application entry point
    ├── stratum-boot/     ← KernelSU module source
    └── out/              ← build output (gitignored)
```

To add a new device, fork the repo and create `devices/<model>/` with `StratumConfig.h` and `app.cpp`. Use `src/default.cpp` as a starting point for `app.cpp`.

---

## Building

```bash
bash scripts/build.sh <device>          # build everything
bash scripts/build.sh <device> -l       # lib only (no examples)
bash scripts/build.sh <device> -e       # examples only
bash scripts/build.sh <device> piano    # specific example(s)
```

Output goes to `devices/<device>/out/`. A flashable KernelSU module zip is produced at `devices/<device>/out/<device>-stratum-boot.zip`.

Requires Clang targeting `aarch64-linux-android34` and AOSP private headers included under `include/`.

---

## Running

```bash
bash scripts/run.sh <device>                  # run your app
bash scripts/run_example.sh <device> <app>    # run a specific example
```

---

## API

```cpp
Stratum s;
s.init();
s.onFrame([](float t) { /* t = seconds since run() */ });
s.onKey([](const KeyEvent& e) { /* e.code, e.action, e.time */ });
s.onTouch([](const TouchEvent& e) { /* e.x, e.y, normalised 0..1 */ });
s.run();   // blocks until stop()
s.stop();  // safe to call from any callback

s.width();    // px
s.height();   // px
s.aspect();   // width / height

Stratum::vibrate(ms);
Stratum::setBrightness(0..255);
Stratum::log(tag, fmt, ...);
```

### KeyEvent

| Field | Type | Notes |
|---|---|---|
| `code` | `int` | Linux keycode (`KEY_POWER`, `KEY_VOLUMEUP`, ...) |
| `action` | `KeyAction` | `DOWN`, `UP`, `REPEAT` |
| `time` | `float` | `CLOCK_MONOTONIC` seconds |

Software repeat fires after `0.5s`, then every `~33ms`.

### TouchEvent

| Field | Type | Notes |
|---|---|---|
| `slot` | `int` | Multitouch slot |
| `id` | `int` | Tracking ID |
| `action` | `TouchAction` | `DOWN`, `MOVE`, `UP` |
| `x`, `y` | `float` | Normalised `0.0..1.0` |
| `time` | `float` | `CLOCK_MONOTONIC` seconds |

---

## Configuration

`devices/<model>/StratumConfig.h` — device-specific paths and touch parameters:

```cpp
namespace StratumConfig {
    const char* TOUCH_DEVICE   = "/dev/input/eventX";
    const char* KEY_DEVICE     = "/dev/input/eventY";
    const char* KEY_DEVICE2    = "";        // optional
    int  TOUCH_PROTOCOL        = 0;         // 0 = auto, 1 = proto A, 2 = proto B
    int  TOUCH_SLOTS           = 10;
    int  TOUCH_X_MIN = 0, TOUCH_X_MAX = 0; // 0 = read from device at runtime
    int  TOUCH_Y_MIN = 0, TOUCH_Y_MAX = 0;
    const char* VIBRATOR       = "/sys/class/timed_output/vibrator/enable";
    const char* BRIGHTNESS     = "/sys/class/leds/lcd-backlight/brightness";
    int  BRIGHTNESS_MIN = 0, BRIGHTNESS_MAX = 255;
}
```

---

## Helpers

**StratumText.h** — 8x8 bitmap font, GL texture atlas, no external files.

```cpp
Text::init(s.aspect(), prog);
Text::draw(str, x, y, size, r, g, b);
Text::drawWrapped(str, x, y, size, maxW, r, g, b);
float cw = Text::charW(size, aspect);
```

**StratumArgs.h** — timeout argument parser for examples.

```cpp
float t = parseTimeout(argc, argv);
// accepts: N, -t N, -t=N, --timeout=N, --help
// default: 0 (unlimited)
```

---

## Examples

Examples are maintained in a separate repository and linked here as a submodule at `apps/`:

```bash
git submodule update --init
```

See [stratum-apps](https://github.com/BroBordd/stratum-apps) for the full list.

---

## KernelSU Module

`devices/<model>/stratum-boot/` is a per-device KernelSU module. It installs the runtime to `/system/bin` and `/system/lib64` and launches `stratum_binary` early via `post-fs-data.sh`, before the Android framework starts.

The flashable zip is built automatically and output to `devices/<model>/out/<model>-stratum-boot.zip`.

---

## Requirements

- Root (KernelSU or Magisk)
- Clang targeting `aarch64-linux-android34`

---

## License

Copyright (C) 2026 BrotherBoard — GNU General Public License v3.0. See [LICENSE](LICENSE).
