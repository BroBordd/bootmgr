#include "Stratum.h"
#include "StratumText.h"
#include "StratumArgs.h"
#include <GLES2/gl2.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

static const char* VSH = R"(
attribute vec2 pos;
void main() { gl_Position = vec4(pos, 0.0, 1.0); }
)";

static const char* FSH = R"(
precision mediump float;
uniform vec4 color;
void main() { gl_FragColor = color; }
)";

int main(int argc, char** argv) {
    float timeout = parseTimeout(argc, argv);

    Stratum s;
    if (!s.init()) return 1;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VSH, nullptr); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FSH, nullptr); glCompileShader(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDetachShader(prog, vs); glDeleteShader(vs);
    glDetachShader(prog, fs); glDeleteShader(fs);
    glUseProgram(prog);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Text::init(s.aspect(), prog);

    s.onFrame([&](float t) {
        if (timeout > 0 && t > timeout) { s.stop(); return; }

        float asp = s.aspect();
        glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float ts = fminf(0.040f, 0.70f * asp / 18);
        Text::draw("NO BINARY CONFIGURED",
                   0.05f, 0.44f,
                   ts, 0.40f, 0.70f, 1.00f);

        float ss = fminf(0.022f, 0.70f * asp / 44);
        Text::drawWrapped("replace stratum_binary in module/system/bin",
                   0.05f, 0.44f + ts + 0.012f,
                   ss, 0.90f, 0.22f, 0.35f, 0.55f);
    });

    s.onKey([&](const KeyEvent& e) {
        if (e.code == KEY_POWER && e.action == KeyAction::DOWN)
            s.stop();
    });

    s.run();
    _exit(0);
}
