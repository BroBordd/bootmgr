#pragma once
#include <string>
#include <stdint.h>
#include <utils/StrongPointer.h>
#include <binder/IBinder.h>
namespace android {
    struct InputApplicationInfo {
        std::string name;
        sp<IBinder> token;
        int64_t dispatchingTimeoutMillis = 5000;
    };
}
