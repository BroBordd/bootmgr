#pragma once

namespace StratumConfig {

    // input devices
    constexpr const char* INPUT_DIR    = "/dev/input";
    constexpr const char* TOUCH_DEVICE = "/dev/input/event2";
    constexpr const char* KEY_DEVICE   = "/dev/input/event3"; // volume up
    constexpr const char* KEY_DEVICE2  = "/dev/input/event0"; // gpio_keys: power + volume down

    // touch protocol: 0 = auto-detect, 1 = force A, 2 = force B
    constexpr int TOUCH_PROTOCOL = 2; // confirmed Protocol B

    // touch axis ranges
    constexpr int TOUCH_X_MIN = 0;
    constexpr int TOUCH_X_MAX = 1080;
    constexpr int TOUCH_Y_MIN = 0;
    constexpr int TOUCH_Y_MAX = 2408;
    constexpr int TOUCH_SLOTS = 10;

    // vibrator — verify: find /sys/class -name "*vibrat*" 2>/dev/null
    constexpr const char* VIBRATOR = "/sys/class/leds/vibrator/activate";

    // brightness — verify: ls /sys/class/backlight/
    constexpr const char* BRIGHTNESS     = "/sys/class/backlight/panel0-backlight/brightness";
    constexpr int         BRIGHTNESS_MIN = 0;
    constexpr int         BRIGHTNESS_MAX = 255;
}
