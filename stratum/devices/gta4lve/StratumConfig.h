#pragma once

namespace StratumConfig {

    // sdk
    constexpr int SDK_VERSION = 31;

    // input devices
    constexpr const char* INPUT_DIR       = "/dev/input";
    constexpr const char* TOUCH_DEVICE    = "/dev/input/event4";
    constexpr const char* KEY_DEVICE      = "/dev/input/event3"; // s2mpu12-power-keys
    constexpr const char* KEY_DEVICE2     = "/dev/input/event3"; // gpio_keys

    // touch protocol: 0 = auto-detect, 1 = force protocol A, 2 = force protocol B
    constexpr int TOUCH_PROTOCOL = 1;

    // touch axis ranges (0 = read from EVIOCGABS at runtime)
    constexpr int TOUCH_X_MIN = 0;
    constexpr int TOUCH_X_MAX = 1200;
    constexpr int TOUCH_Y_MIN = 0;
    constexpr int TOUCH_Y_MAX = 2000;
    constexpr int TOUCH_SLOTS = 10;

    // display orientation: 0 = portrait, 1 = landscape
    constexpr int ORIENTATION = 1;

    // vibrator
    constexpr const char* VIBRATOR        = "/sys/class/timed_output/vibrator/enable";

    // brightness
    constexpr const char* BRIGHTNESS      = "/sys/class/backlight/panel/brightness";
    constexpr int         BRIGHTNESS_MIN  = 0;
    constexpr int         BRIGHTNESS_MAX  = 255;
}

