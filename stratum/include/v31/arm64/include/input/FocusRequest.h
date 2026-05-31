#pragma once
#include <string>
#include <stdint.h>
namespace android {
    struct FocusRequest {
        std::string token;
        std::string windowName;
        int64_t timestamp = 0;
    };
}
