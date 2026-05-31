#include "Stratum.h"
#include "StratumText.h"
#include <GLES2/gl2.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <math.h>
#include <mutex>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

static float mono_now() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9f;
}

static const char* VSH = R"(
attribute vec2 pos;
void main() { gl_Position = vec4(pos, 0.0, 1.0); }
)";

static const char* FSH = R"(
precision mediump float;
uniform vec4 color;
void main() { gl_FragColor = color; }
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

static GLint gPosLoc   = -1;
static GLint gColorLoc = -1;

static void drawRect(float x0, float y0, float x1, float y1,
                     float r, float g, float b, float a = 1.0f) {
    float nx0=x0*2-1, nx1=x1*2-1, ny0=1-y0*2, ny1=1-y1*2;
    float v[] = {nx0,ny0, nx1,ny0, nx0,ny1, nx1,ny1};
    glUniform4f(gColorLoc, r, g, b, a);
    glVertexAttribPointer(gPosLoc, 2, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(gPosLoc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void drawBorder(float x0, float y0, float x1, float y1,
                       float asp, float r, float g, float b, float a = 1.0f) {
    float bh = 0.005f;
    float bw = 0.005f / asp;
    drawRect(x0, y0, x1, y0+bh, r, g, b, a); // top
    drawRect(x0, y1-bh, x1, y1, r, g, b, a); // bottom
    drawRect(x0, y0, x0+bw, y1, r, g, b, a); // left
    drawRect(x1-bw, y0, x1, y1, r, g, b, a); // right
}

struct BootEntry {
    std::string name;
    std::string path;
};

static std::vector<BootEntry> gEntries;
static const char* ENTRIES_DIR = getenv("BOOTMGR_ENTRIES") ? getenv("BOOTMGR_ENTRIES") : "/data/adb/modules/bootmgr/entries";
static const char* LIB_DIR     = getenv("LIB_DIR")         ? getenv("LIB_DIR")         : "/data/adb/modules/bootmgr/system/lib64";

static void loadEntries() {
    gEntries.clear();
    DIR* d = opendir(ENTRIES_DIR);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            gEntries.push_back({e->d_name, std::string(ENTRIES_DIR) + "/" + e->d_name});
        }
        closedir(d);
        
        // Sort alphabetically so that e.g. "01_Android" goes first
        std::sort(gEntries.begin(), gEntries.end(), [](const BootEntry& a, const BootEntry& b) {
            return a.name < b.name;
        });
    }
    
    // Fallback if no entries found
    if (gEntries.empty()) {
        gEntries.push_back({"Android", ""});
    }
}

static void execEntry(const BootEntry& ent) {
    if (!ent.path.empty()) {
        char buf[2048];
        
        snprintf(buf, sizeof(buf), 
            "/system/bin/sh -c '"
            "export LD_LIBRARY_PATH=%s:/system/lib64:/vendor/lib64:/data/data/com.termux/files/usr/lib; "
            "export LD_PRELOAD=%s/stub.so:/data/data/com.termux/files/usr/lib/libX11.so:/data/data/com.termux/files/usr/lib/libXext.so:/data/data/com.termux/files/usr/lib/libXtst.so; "
            "%s'", 
            LIB_DIR, LIB_DIR, ent.path.c_str());
            
        system(buf);
    }
}

int main(int argc, char** argv) {
    loadEntries();

    Stratum s;
    if (!s.init()) return 1;

    GLuint vs   = compileShader(GL_VERTEX_SHADER,   VSH);
    GLuint fs   = compileShader(GL_FRAGMENT_SHADER, FSH);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDetachShader(prog, vs); glDeleteShader(vs);
    glDetachShader(prog, fs); glDeleteShader(fs);
    glUseProgram(prog);

    gPosLoc   = glGetAttribLocation(prog,  "pos");
    gColorLoc = glGetUniformLocation(prog, "color");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Text::init(s.aspect(), prog);

    std::mutex mtx;
    int   selectedIdx     = 0;
    bool  userInteracted  = false;
    float timeout         = 8.0f;
    float startTime       = mono_now();
    float flashT          = -1.f;
    int   flashIdx        = -1;
    
    s.onKey([&](const KeyEvent& e) {
        if (e.action != KeyAction::DOWN && e.action != KeyAction::REPEAT) return;
        Stratum::vibrate(22);
        std::lock_guard<std::mutex> lk(mtx);
        userInteracted = true;
        
        int total = (int)gEntries.size();
        if (e.code == KEY_VOLUMEUP)   selectedIdx = (selectedIdx - 1 + total) % total;
        if (e.code == KEY_VOLUMEDOWN) selectedIdx = (selectedIdx + 1) % total;
        if (e.code == KEY_POWER && flashIdx < 0) {
            flashIdx = selectedIdx;
            flashT = mono_now();
        }
    });

    s.onTouch([&](const TouchEvent& e) {
        if (e.action != TouchAction::DOWN) return;
        Stratum::vibrate(28);
        std::lock_guard<std::mutex> lk(mtx);
        userInteracted = true;
        
        int total = (int)gEntries.size();
        float startY = 0.25f;
        float itemH  = 0.12f;
        float gap    = 0.02f;
        
        for (int i = 0; i < total; i++) {
            float y0 = startY + i * (itemH + gap);
            float y1 = y0 + itemH;
            if (e.y >= y0 && e.y <= y1 && e.x >= 0.08f && e.x <= 0.92f) {
                if (selectedIdx == i && flashIdx < 0) {
                    flashIdx = selectedIdx;
                    flashT = mono_now();
                } else {
                    selectedIdx = i;
                }
                break;
            }
        }
    });

    s.onFrame([&](float t) {
        float now = mono_now();

        int   curSelected;
        bool  curInteracted;
        int   curFlashIdx;
        float curFlashT;
        {
            std::lock_guard<std::mutex> lk(mtx);
            curSelected   = selectedIdx;
            curInteracted = userInteracted;
            curFlashIdx   = flashIdx;
            curFlashT     = flashT;
        }

        // Auto-boot if timeout expires without interaction
        if (!curInteracted && (now - startTime) >= timeout) {
            execEntry(gEntries[0]);
            s.stop();
            return;
        }

        // Execute selection after a visual flash delay
        if (curFlashIdx >= 0 && (now - curFlashT) > 0.15f) {
            execEntry(gEntries[curFlashIdx]);
            s.stop();
            return;
        }

        float asp = s.aspect();

        // Windows 10 Metro Blue Background (#0078D7 roughly)
        glClearColor(0.00f, 0.47f, 0.84f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);

        // Header Title
        float hdrSize = fminf(0.045f, 0.70f * asp / 26);
        Text::draw("Choose an operating system", 0.08f, 0.10f, hdrSize, 1.0f, 1.0f, 1.0f);

        // Draw OS Entries (Tiles)
        float startY = 0.25f;
        float itemH  = 0.12f;
        float gap    = 0.02f;
        int   total  = (int)gEntries.size();

        for (int i = 0; i < total; i++) {
            float y0 = startY + i * (itemH + gap);
            float y1 = y0 + itemH;
            bool isSel = (i == curSelected);

            if (isSel) {
                // Highlighted Tile (Slightly lighter blue + White Border)
                drawRect(0.08f, y0, 0.92f, y1, 0.15f, 0.55f, 0.86f, 1.0f);
                drawBorder(0.08f, y0, 0.92f, y1, asp, 1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                // Dim Tile
                drawRect(0.08f, y0, 0.92f, y1, 0.00f, 0.40f, 0.75f, 1.0f);
            }

            // Cleanup filename for display (removes e.g. "01_")
            std::string dispName = gEntries[i].name;
            if (dispName.length() > 3 && dispName[2] == '_') {
                dispName = dispName.substr(3);
            }

            // Draw Tile Text
            float txtSize = fminf(0.035f, 0.75f * asp / dispName.length());
            float txtY = y0 + (itemH - txtSize) * 0.5f;
            Text::draw(dispName.c_str(), 0.12f, txtY, txtSize, 1.0f, 1.0f, 1.0f);
        }

        // Draw Timer or Hint at the bottom
        float btmSize = fminf(0.025f, 0.80f * asp / 40);
        if (!curInteracted) {
            int secs = (int)ceil(timeout - (now - startTime));
            if (secs < 0) secs = 0;
            
            std::string defName = gEntries[0].name;
            if (defName.length() > 3 && defName[2] == '_') defName = defName.substr(3);

            char buf[128];
            snprintf(buf, sizeof(buf), "Starting %s in %d seconds", defName.c_str(), secs);
            Text::draw(buf, 0.08f, 0.88f, btmSize, 0.85f, 0.90f, 0.95f);
        } else {
            const char* hint = "Use Volume to select, Power to confirm";
            Text::draw(hint, 0.08f, 0.88f, btmSize, 0.85f, 0.90f, 0.95f);
        }
    });

    s.run();
    _exit(0);
}
