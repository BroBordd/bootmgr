#define __BIONIC_AVAILABILITY_GUARD(x) 1

#pragma clang diagnostic ignored "-Winvalid-specialization"

// v31-specific workarounds — block problematic input headers that conflict
#if __ANDROID_API__ < 33
#define ANDROID_GUI_INPUTWINDOW_H
#define ANDROID_INPUT_INPUTWINDOW_H
#define ANDROID_INPUT_INPUTAPPLICATION_H
#define ANDROID_FOCUSREQUEST_H
#define ANDROID_INPUTAPPLICATIONINFO_H
#define _LIBINPUT_INPUT_H
#define _LIBINPUT_INPUT_TRANSPORT_H
#define _UI_INPUT_WINDOW_H
#define NO_INPUT

#include <utils/StrongPointer.h>
namespace android {
    class InputWindowHandle;
    struct InputWindowInfo;
    struct FocusRequest;
}
#endif
