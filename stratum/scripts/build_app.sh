#!/bin/bash
set -e

# usage: build_app.sh <device> <file.cpp> [output_name]
# builds a single .cpp against stratum and outputs to devices/<device>/out/bins/

ROOT=$(dirname $0)/..

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "usage: $0 <device> <file.cpp> [output_name]"
    exit 1
fi

DEVICE=$1
SRC=$2
NAME=${3:-$(basename "$SRC" .cpp)}

DEVICE_DIR="$ROOT/devices/$DEVICE"
OUT_BINS="$DEVICE_DIR/out/bins"
OUT_LIBS="$DEVICE_DIR/out/libs"

if [ ! -d "$DEVICE_DIR" ]; then
    echo "error: '$DEVICE_DIR' not found"
    exit 1
fi

if [ ! -f "$DEVICE_DIR/StratumConfig.h" ]; then
    echo "error: missing $DEVICE_DIR/StratumConfig.h"
    exit 1
fi

if [ ! -f "$SRC" ]; then
    echo "error: source file '$SRC' not found"
    exit 1
fi

if [ ! -f "$OUT_LIBS/libstratum.so" ]; then
    echo "error: libstratum.so not found in $OUT_LIBS — run build.sh -l first"
    exit 1
fi

mkdir -p "$OUT_BINS"

INCLUDES="\
  -I$DEVICE_DIR \
  -I$ROOT/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/gui/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/binder/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/ui/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/math/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/nativewindow/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/libs/nativebase/include \
  -I$ROOT/include/v34/arm64/include/frameworks/native/opengl/include \
  -I$ROOT/include/v34/arm64/include/system/libbase/include \
  -I$ROOT/include/v34/arm64/include/system/core/libutils/include \
  -I$ROOT/include/v34/arm64/include/system/core/libcutils/include \
  -I$ROOT/include/v34/arm64/include/system/core/libsystem/include \
  -I$ROOT/include/v34/arm64/include/system/libhidl/base/include \
  -I$ROOT/include/v34/arm64/include/system/libhidl/transport/token/1.0/utils/include \
  -I$ROOT/include/v34/arm64/include/system/libfmq/base \
  -I$ROOT/include/v34/arm64/include/hardware/libhardware/include \
  -I$ROOT/include/v34/arm64/include/generated-headers/frameworks/native/libs/gui/libgui_aidl_static/android_vendor.34_arm64_armv8-a_static/gen/aidl \
  -I$ROOT/include/v34/arm64/include/generated-headers/frameworks/native/libs/gui/libgui/android_vendor.34_arm64_armv8-a_shared/gen/aidl \
  -I$ROOT/include/v34/arm64/include/generated-headers/frameworks/native/libs/gui/libgui_window_info_static/android_vendor.34_arm64_armv8-a_static_afdo-libgui_lto-thin/gen/aidl \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V4-ndk-source/gen/include \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/generated-headers/hardware/interfaces/media/1.0/android.hardware.media@1.0_genc++_headers/gen \
  -I$ROOT/include/v34/arm64/include/system/libhwbinder/include \
  -I$ROOT/include/logging/liblog/include"

FLAGS="-std=c++17 -O2 -march=armv8-a -target aarch64-linux-android34 -D__BIONIC__ -frtti -w -include $ROOT/include/compat.h"
LIBS="-L/system/lib64 -lgui -lui -lEGL -lGLESv2 -lbinder -lutils -llog -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-all"

echo "[*] Building $NAME..."
clang++ $FLAGS $INCLUDES \
    -L"$OUT_LIBS" "$SRC" $LIBS -lstratum \
    -o "$OUT_BINS/$NAME"

echo "[*] Done: $OUT_BINS/$NAME"
