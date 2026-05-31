#pragma once
#include <stdlib.h>
#include <stdio.h>

inline float parseTimeout(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            printf("usage: <example> [timeout]\n");
            printf("  timeout   seconds to run (0 = unlimited)\n");
            printf("  -t N      same as passing N directly\n");
            printf("  --timeout=N\n");
            exit(0);
        }
        if (a[0] == '-' && a[1] == 't' && a[2] == '=') return atof(a + 3);
        if (a[0] == '-' && a[1] == '-') {
            const char* eq = a + 2;
            while (*eq && *eq != '=') eq++;
            if (*eq == '=') return atof(eq + 1);
        }
        if (i + 1 < argc && a[0] == '-' && a[1] == 't') return atof(argv[++i]);
        if (a[0] >= '0' && a[0] <= '9') return atof(a);
    }
    return 0.0f; // unlimited by default
}
