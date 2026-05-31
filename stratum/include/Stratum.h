#pragma once
#include <functional>
#include <memory>

enum class KeyAction { DOWN, UP, REPEAT };

struct KeyEvent {
    int       code;
    KeyAction action;
    float     time;
};

enum class TouchAction { DOWN, MOVE, UP };

struct TouchEvent {
    int         slot;
    int         id;
    TouchAction action;
    float       x, y;
    float       time;
};

class Stratum {
public:
    Stratum();
    ~Stratum();

    bool init();
    void onFrame(std::function<void(float t)> cb);
    void onKey(std::function<void(const KeyEvent&)> cb);
    void onTouch(std::function<void(const TouchEvent&)> cb);
    void stop();
    void run();

    int   width()  const;
    int   height() const;
    float aspect() const;

    static void vibrate(int ms);
    static void setBrightness(int val);   // 0..255
    static void log(const char* tag, const char* fmt, ...);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
