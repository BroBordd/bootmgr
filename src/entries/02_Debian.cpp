#include "Stratum.h"
#include "StratumConfig.h"
#include <GLES2/gl2.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <algorithm>
#include <mutex>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#define TERMUX_PREFIX "/data/data/com.termux/files/usr"
#define TERMUX_TMP    TERMUX_PREFIX "/tmp"
#define PROOT_BIN     TERMUX_PREFIX "/bin/proot"
#define DEBIAN_ROOTFS TERMUX_PREFIX "/var/lib/proot-distro/installed-rootfs/debian"

//#define X11_SOCK    TERMUX_TMP "/.X11-unix/X0"
//#define X11_LOCK    TERMUX_TMP "/.X0-lock"
//#define X11_SOCKDIR TERMUX_TMP "/.X11-unix"
#define X11_SOCK    DEBIAN_ROOTFS "/tmp/.X11-unix/X0"
#define X11_LOCK    DEBIAN_ROOTFS "/tmp/.X0-lock"
#define X11_SOCKDIR DEBIAN_ROOTFS "/tmp/.X11-unix"

// --- Gesture Tuning ---
#define TAP_MAX_SECS   0.18f   
#define TAP_MAX_PX     10.f    
#define LONGPRESS_SECS 0.40f   
#define SWIPE_SPEED    1.2f
#define SCROLL_PX      40.f    

static pid_t gLinuxPid = -1;

// --- Thread-Safe X11 & Display ---
static std::mutex gX11Mutex;
static Display* gDisplay  = nullptr;
static int      gFbFd     = -1;
static uint8_t* gFileMap  = nullptr;
static size_t   gFileSize = 0;
static uint8_t* gFbMap    = nullptr;
static int      gFbPitch  = 0;
static GLuint   gTex      = 0;
static int      gXW       = 0;
static int      gXH       = 0;
static bool     gBridgeOK = false;

// --- Mode Switching State ---
static bool  gIsAbsoluteMode = true; // True = Touchscreen, False = Trackpad
static bool  gIgnoreUntilZero = false; // Blocks clicks right after switching modes
static float gHudTimer = 0.0f; // Controls the GL Overlay fade animation

// --- Raw Finger Tracking (For Gesture Detection) ---
struct Finger {
    bool active = false;
    float x = 0.f, y = 0.f;
    float downX = 0.f, downY = 0.f;
    float downT = 0.f;
};
static Finger gFingers[10];

static float px_dist(float nx0, float ny0, float nx1, float ny1) {
    return sqrtf(powf((nx1 - nx0)*gXW, 2) + powf((ny1 - ny0)*gXH, 2));
}

// ---------------------------------------------------------------
// Thread-Safe X11 Input Injection
// ---------------------------------------------------------------
static float gCurX = 0.f, gCurY = 0.f;

static void x_move_abs(float normX, float normY) {
    std::lock_guard<std::mutex> lock(gX11Mutex);
    if (!gDisplay) return;
    int x = std::clamp((int)(normX * gXW), 0, gXW - 1);
    int y = std::clamp((int)(normY * gXH), 0, gXH - 1);
    gCurX = x; gCurY = y;
    XTestFakeMotionEvent(gDisplay, DefaultScreen(gDisplay), x, y, CurrentTime);
    XFlush(gDisplay);
}

static void x_move_rel(float dx, float dy) {
    std::lock_guard<std::mutex> lock(gX11Mutex);
    if (!gDisplay) return;
    gCurX = std::clamp(gCurX + dx, 0.f, (float)gXW - 1.f);
    gCurY = std::clamp(gCurY + dy, 0.f, (float)gXH - 1.f);
    XTestFakeMotionEvent(gDisplay, DefaultScreen(gDisplay), (int)gCurX, (int)gCurY, CurrentTime);
    XFlush(gDisplay);
}

static void x_button(int btn, bool press) {
    std::lock_guard<std::mutex> lock(gX11Mutex);
    if (!gDisplay) return;
    XTestFakeButtonEvent(gDisplay, btn, press ? True : False, CurrentTime);
    XFlush(gDisplay);
}

static void x_click(int btn) { x_button(btn, true); x_button(btn, false); }

static void x_scroll(int ticks) {
    int btn = ticks > 0 ? 4 : 5;
    for (int i = 0; i < std::abs(ticks); i++) x_click(btn);
}

// ---------------------------------------------------------------
// Absolute Mode State (Touchscreen)
// ---------------------------------------------------------------
static int   gAbsPrimarySlot   = -1;
static int   gAbsSecondarySlot = -1;
static float gAbsSDownY = 0.f, gAbsSDownT = 0.f;
static bool  gAbsSMoved = false;
static float gAbsScrollAcc = 0.f;

void processAbsoluteTouch(const TouchEvent& e) {
    if (e.action == TouchAction::DOWN) {
        if (gAbsPrimarySlot == -1) {
            gAbsPrimarySlot = e.slot;
            x_move_abs(e.x, e.y);
            x_button(1, true); 
        } else if (gAbsSecondarySlot == -1) {
            gAbsSecondarySlot = e.slot;
            gAbsSDownY = e.y;
            gAbsSDownT = e.time;
            gAbsSMoved = false;
            gAbsScrollAcc = 0.f;
            x_button(1, false); // Cancel drag to scroll
        }
    } else if (e.action == TouchAction::MOVE) {
        if (e.slot == gAbsPrimarySlot && gAbsSecondarySlot == -1) {
            x_move_abs(e.x, e.y);
        } else if (e.slot == gAbsSecondarySlot) {
            gAbsScrollAcc += (e.y - gAbsSDownY) * gXH;
            int ticks = (int)(gAbsScrollAcc / SCROLL_PX);
            if (ticks != 0) {
                x_scroll(-ticks);
                gAbsScrollAcc -= ticks * SCROLL_PX;
                gAbsSMoved = true;
            }
            gAbsSDownY = e.y;
        }
    } else if (e.action == TouchAction::UP) {
        if (e.slot == gAbsPrimarySlot) {
            x_move_abs(e.x, e.y);
            if (gAbsSecondarySlot == -1) x_button(1, false);
            gAbsPrimarySlot = -1;
        } else if (e.slot == gAbsSecondarySlot) {
            if (!gAbsSMoved && (e.time - gAbsSDownT) < 0.25f) x_click(3); 
            gAbsSecondarySlot = -1;
        }
    }
}

// ---------------------------------------------------------------
// Relative Mode State (Trackpad)
// ---------------------------------------------------------------
enum class GestureState { IDLE, TRACKING, MOVING, DRAGGING, TWO_FINGER };
static GestureState gRelGesture = GestureState::IDLE;

static float gRelF0DownX = 0.f, gRelF0DownY = 0.f, gRelF0DownT = 0.f;
static float gRelF0PrevX = 0.f, gRelF0PrevY = 0.f;
static float gRelF1PrevY = 0.f, gRelScrollAcc = 0.f;
static bool  gRelF1Active = false;

void processRelativeTouch(const TouchEvent& e) {
    if (e.slot == 0) {
        if (e.action == TouchAction::DOWN) {
            gRelF0DownX = gRelF0PrevX = e.x;
            gRelF0DownY = gRelF0PrevY = e.y;
            gRelF0DownT = e.time;
            gRelScrollAcc = 0.f;
            if (gRelGesture == GestureState::IDLE) gRelGesture = GestureState::TRACKING;
        } else if (e.action == TouchAction::MOVE) {
            if (gRelGesture == GestureState::TRACKING && px_dist(gRelF0DownX, gRelF0DownY, e.x, e.y) > TAP_MAX_PX) 
                gRelGesture = GestureState::MOVING;

            if (gRelGesture == GestureState::MOVING || gRelGesture == GestureState::DRAGGING) {
                x_move_rel((e.x - gRelF0PrevX) * gXW * SWIPE_SPEED, (e.y - gRelF0PrevY) * gXH * SWIPE_SPEED);
            }
            gRelF0PrevX = e.x; gRelF0PrevY = e.y;
        } else if (e.action == TouchAction::UP) {
            if (gRelGesture == GestureState::TRACKING && (e.time - gRelF0DownT) <= TAP_MAX_SECS && px_dist(gRelF0DownX, gRelF0DownY, e.x, e.y) <= TAP_MAX_PX) {
                x_click(1);
            } else if (gRelGesture == GestureState::DRAGGING) {
                x_button(1, false);
            }
            if (!gRelF1Active) gRelGesture = GestureState::IDLE;
        }
    } else if (e.slot == 1) {
        if (e.action == TouchAction::DOWN) {
            gRelF1Active = true;
            gRelF1PrevY = e.y;
            gRelScrollAcc = 0.f;
            gRelGesture = GestureState::TWO_FINGER;
        } else if (e.action == TouchAction::MOVE) {
            if (gRelGesture == GestureState::TWO_FINGER) {
                gRelScrollAcc += (e.y - gRelF1PrevY) * gXH;
                int ticks = (int)(gRelScrollAcc / SCROLL_PX);
                if (ticks != 0) { x_scroll(-ticks); gRelScrollAcc -= ticks * SCROLL_PX; }
            }
            gRelF1PrevY = e.y;
        } else if (e.action == TouchAction::UP) {
            if (gRelGesture == GestureState::TWO_FINGER && (e.time - gRelF0DownT) <= TAP_MAX_SECS && fabsf(gRelScrollAcc) < TAP_MAX_PX) {
                x_click(3);
            }
            gRelF1Active = false;
            gRelGesture = GestureState::TRACKING; 
        }
    }
}

// ---------------------------------------------------------------
// Mode Reset (Clears stuck buttons when switching)
// ---------------------------------------------------------------
void resetTouchStates() {
    x_button(1, false); x_button(3, false);
    gAbsPrimarySlot = -1; gAbsSecondarySlot = -1;
    gRelGesture = GestureState::IDLE; gRelF1Active = false;
}

// ---------------------------------------------------------------
// GPU Bridge & Animated Shader HUD
// ---------------------------------------------------------------
static const char* kVert = R"(
attribute vec2 aPos;
varying vec2 vUV;
void main() {
    // Standard unrotated UV mapping (0,0 is physical top-left)
    vec2 physUV = vec2(aPos.x * 0.5 + 0.5, 0.5 - aPos.y * 0.5);
    
    // Rotate 90-degrees Counter-Clockwise mapping for Landscape mode
    vUV = vec2(physUV.y, 1.0 - physUV.x);
    
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* kFrag = R"(
precision mediump float;
uniform sampler2D uTex;
uniform float uUvScale;
uniform float uHudTime; // Animation Timer (0.0 to 2.0)
uniform float uHudMode; // 1.0 = Touch, 0.0 = Mouse
uniform vec2 uResolution; // Screen dimensions for aspect ratio

varying vec2 vUV;

float sdRoundRect(vec2 p, vec2 b, float r) {
    vec2 d = abs(p) - b + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

void main() {
    vec2 frameUV = vec2(vUV.x * uUvScale, vUV.y);
    vec4 c = texture2D(uTex, frameUV).bgra;

    if (uHudTime > 0.0) {
        float alpha = smoothstep(0.0, 0.2, uHudTime) * smoothstep(2.0, 1.8, uHudTime);
        
        // 1. Convert UV to absolute pixels to avoid squishing
        vec2 px = vUV * uResolution;
        
        // 2. Base size on smallest screen dimension (e.g. 15% of width)
        float minDim = min(uResolution.x, uResolution.y);
        float hudSize = minDim * 0.15; 
        
        // 3. Anchor bottom-left with a nice proportional margin
        float margin = hudSize * 0.2;
        vec2 hudCenter = vec2(margin + hudSize * 0.5, uResolution.y - (margin + hudSize * 0.5));
        
        // 4. Map into the local 'cuv' coordinate system the icon logic uses
        vec2 cuv = (px - hudCenter) / (hudSize * 2.0);
        
        float box = sdRoundRect(cuv, vec2(0.25, 0.25), 0.05);
        if (box < 0.0) {
            c = mix(c, vec4(0.1, 0.1, 0.12, 0.95), alpha); // Dark frosted background
            
            float icon = 0.0;
            if (uHudMode > 0.5) {
                // TOUCH ICON
                float d = length(cuv);
                if (d < 0.12 && abs(sin(d * 60.0 - uHudTime * 15.0)) > 0.6) icon = 1.0;
                if (d < 0.03) icon = 1.0;
            } else {
                // MOUSE ICON (Coordinates updated so mouse buttons face UP)
                float mbody = sdRoundRect(cuv, vec2(0.08, 0.12), 0.06);
                if (mbody < 0.0) {
                    icon = 1.0;
                    if (abs(cuv.x) < 0.01 && cuv.y < 0.0) icon = 0.0; // split buttons
                    if (abs(cuv.y + 0.02) < 0.01) icon = 0.0; // horizontal line
                    if (length(cuv - vec2(0.0, -0.06)) < 0.02) icon = 1.0; // scroll wheel
                }
            }
            c = mix(c, vec4(0.2, 0.8, 1.0, 1.0), icon * alpha); // Bright Cyan Icon
        }
    }
    gl_FragColor = c;
}
)";

static GLuint gProg = 0, gVBO = 0;
static GLint  gaPos = 0, guTex = 0, guUvScale = 0, guHudTime = 0, guHudMode = 0, guResolution = 0;

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static bool bridge_init() {
    setenv("DISPLAY", ":0", 1);
    setenv("XAUTHORITY", "", 1);

    // Point X11 unix socket to chroot tmp
    unlink("/tmp");
    symlink(DEBIAN_ROOTFS "/tmp", "/tmp");
    
    std::lock_guard<std::mutex> lock(gX11Mutex);
    gDisplay = XOpenDisplay(":0");
    if (!gDisplay) return false;

    int fd = open(DEBIAN_ROOTFS "/tmp/Xvfb_screen0", O_RDONLY);

//static bool bridge_init() {
//    if (access("/tmp", F_OK) != 0) symlink(TERMUX_TMP, "/tmp");
//    setenv("DISPLAY", ":0", 1);
//    
//    std::lock_guard<std::mutex> lock(gX11Mutex);
//    gDisplay = XOpenDisplay(":0");
//    if (!gDisplay) return false;
//
//    int fd = open(TERMUX_TMP "/Xvfb_screen0", O_RDONLY);
    if (fd < 0) { XCloseDisplay(gDisplay); gDisplay = nullptr; return false; }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < 4) { close(fd); return false; }

    uint8_t* fmap = (uint8_t*)mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmap == MAP_FAILED) { close(fd); return false; }

    uint32_t header_size = (fmap[0]<<24) | (fmap[1]<<16) | (fmap[2]<<8) | fmap[3];
    uint32_t bpl         = (fmap[48]<<24) | (fmap[49]<<16) | (fmap[50]<<8) | fmap[51]; 
    uint32_t ncolors     = (fmap[76]<<24) | (fmap[77]<<16) | (fmap[78]<<8) | fmap[79];
    uint32_t px_offset   = header_size + (ncolors * 12);
    
    if (px_offset + gXH * bpl > st.st_size) {
        munmap(fmap, st.st_size); close(fd); return false;
    }
    
    gFbFd = fd; gFileSize = st.st_size; gFileMap = fmap;
    gFbMap = fmap + px_offset; gFbPitch = bpl / 4; 

    glGenTextures(1, &gTex);
    glBindTexture(GL_TEXTURE_2D, gTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gProg = glCreateProgram();
    glAttachShader(gProg, compile_shader(GL_VERTEX_SHADER, kVert)); 
    glAttachShader(gProg, compile_shader(GL_FRAGMENT_SHADER, kFrag));
    glLinkProgram(gProg);

    float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
    glGenBuffers(1, &gVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    gaPos        = glGetAttribLocation(gProg, "aPos");
    guTex        = glGetUniformLocation(gProg, "uTex");
    guUvScale    = glGetUniformLocation(gProg, "uUvScale");
    guHudTime    = glGetUniformLocation(gProg, "uHudTime");
    guHudMode    = glGetUniformLocation(gProg, "uHudMode");
    guResolution = glGetUniformLocation(gProg, "uResolution");

    return true;
}

static void bridge_destroy() {
    std::lock_guard<std::mutex> lock(gX11Mutex);
    if (gDisplay) { XCloseDisplay(gDisplay); gDisplay = nullptr; }
    if (gFileMap) { munmap(gFileMap, gFileSize); gFileMap = nullptr; gFbMap = nullptr; }
    if (gFbFd>=0) { close(gFbFd); gFbFd = -1; }
    if (gTex)     { glDeleteTextures(1, &gTex); gTex = 0; }
    gBridgeOK = false;
}

static void bridge_blit(float dt) {
    if (gHudTimer > 0.f) gHudTimer -= dt;

    glBindTexture(GL_TEXTURE_2D, gTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gFbPitch, gXH, 0, GL_RGBA, GL_UNSIGNED_BYTE, gFbMap);

    glUseProgram(gProg);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glEnableVertexAttribArray(gaPos);
    glVertexAttribPointer(gaPos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glUniform1i(guTex, 0);
    glUniform1f(guUvScale, (float)gXW / (float)gFbPitch);
    glUniform1f(guHudTime, std::max(0.0f, gHudTimer));
    glUniform1f(guHudMode, gIsAbsoluteMode ? 1.0f : 0.0f);
    glUniform2f(guResolution, (float)gXW, (float)gXH);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// ---------------------------------------------------------------
// Linux Process Management
// ---------------------------------------------------------------
//static void cleanup_x11_socket() {
//    unlink(X11_SOCK); unlink(X11_LOCK); rmdir(X11_SOCKDIR);
//    unlink(TERMUX_TMP "/Xvfb_screen0");
//}

static void cleanup_x11_socket() {
    unlink(X11_SOCK); unlink(X11_LOCK); rmdir(X11_SOCKDIR);
    unlink(DEBIAN_ROOTFS "/tmp/Xvfb_screen0");
}

static void kill_linux() {
    if (gLinuxPid > 0) {
        killpg(gLinuxPid, SIGTERM);
        sleep(1); 
        killpg(gLinuxPid, SIGKILL);
        waitpid(gLinuxPid, nullptr, 0); 
        gLinuxPid = -1;
    }
    cleanup_x11_socket();
}

static void launch_linux(int w, int h) {
    cleanup_x11_socket();
    pid_t pid = fork();
    if (pid == 0) {
        setsid(); setpgid(0, 0);
        unsetenv("LD_PRELOAD"); unsetenv("LD_LIBRARY_PATH");
        setenv("PATH", "/data/data/com.termux/files/usr/bin:/sbin:/bin:/usr/bin", 1);
        setenv("TMPDIR", TERMUX_TMP, 1);

        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "su -c '"
            "mount --bind /dev " DEBIAN_ROOTFS "/dev && "
            "mount --bind /proc " DEBIAN_ROOTFS "/proc && "
            "mount --bind /sys " DEBIAN_ROOTFS "/sys && "
            "mount --bind /system " DEBIAN_ROOTFS "/system && "
            "mount -t devpts devpts " DEBIAN_ROOTFS "/dev/pts && "
            "mount --bind " DEBIAN_ROOTFS "/tmp " TERMUX_TMP " && "
            "chroot " DEBIAN_ROOTFS " /usr/bin/env -i "
            "HOME=/root TERM=xterm PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin "
            "/bin/sh -c \""
            "mkdir -p /tmp/.X11-unix && "
            "chmod 1777 /tmp/.X11-unix && "
            "Xvfb :0 -screen 0 %dx%dx24 -fbdir /tmp > /tmp/xvfb.log 2>&1 & "
            "sleep 1 && DISPLAY=:0 xsetroot -cursor_name left_ptr & "
            "DISPLAY=:0 matchbox-keyboard & "
            "DISPLAY=:0 dbus-launch --exit-with-session startxfce4 > /tmp/xfce.log 2>&1"
            "\"'",
        w, h);

        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(1);
    } else if (pid > 0) {
        gLinuxPid = pid;
    }
}

// ---------------------------------------------------------------
// Main
// ---------------------------------------------------------------
int main(int argc, char** argv) {
    // Safe to stop - big RAM wins
    system("setprop ctl.stop zygote");
    system("setprop ctl.stop bootanim");
    system("setprop ctl.stop audioserver");
    system("setprop ctl.stop cameraserver");
    system("setprop ctl.stop media");
    system("setprop ctl.stop mediadrm");
    system("setprop ctl.stop statsd");
    system("setprop ctl.stop tombstoned");
    system("setprop ctl.stop lmkd");

    // Drop caches after zygote is dead
    system("echo 3 > /proc/sys/vm/drop_caches");
    sleep(2);

    XInitThreads();
    setenv("TMPDIR", TERMUX_TMP, 1);
    cleanup_x11_socket();

    Stratum s;
    if (!s.init()) return 1;

    // Map hardware physical properties horizontally to setup X11
    gXW = s.height();
    gXH = s.width();
    launch_linux(gXW, gXH);

    s.onKey([&](const KeyEvent& e) {
        if (e.action != KeyAction::DOWN) return;
        if (e.code == KEY_POWER) s.stop();
    });

    s.onTouch([&](const TouchEvent& raw_e) {
        if (!gBridgeOK) return;

        // Translate the raw physical portrait touch into a logical landscape touch space
        TouchEvent e = raw_e;
        e.x = raw_e.y;
        e.y = 1.0f - raw_e.x;

        // 1. Maintain Raw Finger Tracking for Global Gestures
        Finger& f = gFingers[e.slot];
        if (e.action == TouchAction::DOWN) {
            f.active = true;
            f.x = f.downX = e.x;
            f.y = f.downY = e.y;
            f.downT = e.time;
        } else if (e.action == TouchAction::MOVE) {
            f.x = e.x; f.y = e.y;
        } else if (e.action == TouchAction::UP) {
            f.active = false;
        }

        // 2. Block inputs until all fingers lift after a mode switch
        if (gIgnoreUntilZero) {
            bool anyActive = false;
            for (int i = 0; i < 10; i++) if (gFingers[i].active) anyActive = true;
            if (!anyActive) gIgnoreUntilZero = false;
            return;
        }

        // 3. Route to Active Touch Mode
        if (gIsAbsoluteMode) processAbsoluteTouch(e);
        else processRelativeTouch(e);
    });

    s.onFrame([&](float t) {
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        static float lastT = t;
        float dt = t - lastT;
        lastT = t;

        // Mode Switch Detection: 2 Fingers Down & Held Still for 0.8s
        if (!gIgnoreUntilZero) {
            int activeCount = 0;
            float maxDist = 0.f;
            for (int i = 0; i < 10; i++) {
                if (gFingers[i].active) {
                    activeCount++;
                    maxDist = std::max(maxDist, px_dist(gFingers[i].downX, gFingers[i].downY, gFingers[i].x, gFingers[i].y));
                }
            }
            
            // Accumulate dt directly instead of relying on mismatching timebases
            static float holdTimer = 0.f;
            if (activeCount == 2 && maxDist < 30.f) {
                holdTimer += dt;
                if (holdTimer > 0.8f) {
                    gIsAbsoluteMode = !gIsAbsoluteMode;
                    gHudTimer = 2.0f; // Trigger GL Overlay Animation
                    resetTouchStates();
                    gIgnoreUntilZero = true;
                    holdTimer = 0.f; // Reset for next use
                }
            } else {
                holdTimer = 0.f;
            }
        }

        // Long Press Trackpad Dragging Detection (Relative Mode Only)
        // Accumulating dt safely handles different hardware/driver timebases.
        static float trackpadHoldTimer = 0.f;
        if (!gIsAbsoluteMode && gRelGesture == GestureState::TRACKING && gBridgeOK) {
            if (px_dist(gRelF0DownX, gRelF0DownY, gRelF0PrevX, gRelF0PrevY) <= TAP_MAX_PX) {
                trackpadHoldTimer += dt;
                if (trackpadHoldTimer >= LONGPRESS_SECS) {
                    x_button(1, true);
                    gRelGesture = GestureState::DRAGGING;
                    trackpadHoldTimer = 0.f;
                }
            } else {
                trackpadHoldTimer = 0.f;
            }
        } else {
            trackpadHoldTimer = 0.f;
        }

        if (!gBridgeOK && t >= 3.0f) {
            gBridgeOK = bridge_init();
            if (gBridgeOK) x_move_abs(0.5f, 0.5f); 
        }

        if (gBridgeOK) bridge_blit(dt);

        static float lastCheck = 0.f;
        if (t - lastCheck >= 5.f && gLinuxPid > 0) {
            lastCheck = t;
            if (waitpid(gLinuxPid, nullptr, WNOHANG) == gLinuxPid) gLinuxPid = -1;
        }
    });

    s.run();
    
    resetTouchStates();
    bridge_destroy();
    kill_linux();
    return 0;
}
