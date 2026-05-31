#!/bin/bash
set -e

ROOT=$(dirname $0)/..

usage() {
    echo "usage: $0 <device> [options] [app...]"
    echo "  -l, --lib-only       build lib + app only, skip examples"
    echo "  -e, --examples-only  skip rebuilding lib (examples only)"
    echo "  -h, --help           show this help"
    echo ""
    echo "  app args: specific example name(s) to build (default: all)"
    echo "  e.g: $0 a14 terminal brickbreaker"
    exit 0
}

# ── device guard ─────────────────────────────────────────────────────────────
if [ -z "$1" ] || [[ "$1" == -* ]]; then
    echo "error: device not specified"
    echo "usage: $0 <device> [options]"
    exit 1
fi

DEVICE=$1
shift

DEVICE_DIR="$ROOT/devices/$DEVICE"

if [ ! -d "$DEVICE_DIR" ]; then
    echo "error: '$DEVICE_DIR' not found — fork and add your device folder"
    exit 1
fi

if [ ! -f "$DEVICE_DIR/StratumConfig.h" ]; then
    echo "error: missing $DEVICE_DIR/StratumConfig.h"
    exit 1
fi

if [ ! -d "$DEVICE_DIR/stratum-boot" ]; then
    echo "error: missing $DEVICE_DIR/stratum-boot/"
    exit 1
fi

# ── output dirs ───────────────────────────────────────────────────────────────
OUT_BINS="$DEVICE_DIR/out/bins"
OUT_LIBS="$DEVICE_DIR/out/libs"
mkdir -p "$OUT_BINS" "$OUT_LIBS"

# ── args ──────────────────────────────────────────────────────────────────────
BUILD_LIB=1
BUILD_EXAMPLES=1
ONLY=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -l|--lib-only)       BUILD_EXAMPLES=0; shift ;;
        -e|--examples-only)  BUILD_LIB=0;      shift ;;
        -h|--help)           usage ;;
        -*) echo "unknown option: $1"; usage ;;
        *)  ONLY+=("$1");    shift ;;
    esac
done

# ── detect SDK version from StratumConfig.h ───────────────────────────────────
SDK=$(grep 'SDK_VERSION' "$DEVICE_DIR/StratumConfig.h" | grep -o '[0-9]*' | head -1)
SDK=${SDK:-34}
echo "[*] SDK version: $SDK"

# ── sdk-specific aidl path ────────────────────────────────────────────────────
if [ "$SDK" -ge 33 ]; then
    AIDL_COMMON="android.hardware.graphics.common-V4-ndk-source"
else
    AIDL_COMMON="android.hardware.graphics.common-V2-ndk_platform-source"
fi

INCLUDES="\
  -I$DEVICE_DIR \
  -I$ROOT/include \
  -I$ROOT/include/v$SDK/arm64/include/external/fmtlib/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/gui/include \
  -I$ROOT/include/v$SDK/arm64/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/binder/ndk/include_cpp \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/binder/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/ui/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/math/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/nativewindow/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/libs/nativebase/include \
  -I$ROOT/include/v$SDK/arm64/include/frameworks/native/opengl/include \
  -I$ROOT/include/v$SDK/arm64/include/system/libbase/include \
  -I$ROOT/include/v$SDK/arm64/include/system/core/libutils/include \
  -I$ROOT/include/v$SDK/arm64/include/system/core/libcutils/include \
  -I$ROOT/include/v$SDK/arm64/include/system/core/libsystem/include \
  -I$ROOT/include/v$SDK/arm64/include/system/libhidl/base/include \
  -I$ROOT/include/v$SDK/arm64/include/system/libhidl/transport/token/1.0/utils/include \
  -I$ROOT/include/v$SDK/arm64/include/system/libfmq/base \
  -I$ROOT/include/v$SDK/arm64/include/hardware/libhardware/include \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/frameworks/native/libs/gui/libgui_aidl_static/android_vendor.${SDK}_arm64_armv8-a_static/gen/aidl \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/frameworks/native/libs/gui/libgui/android_vendor.${SDK}_arm64_armv8-a_shared/gen/aidl \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/frameworks/native/libs/gui/libgui_window_info_static/android_vendor.${SDK}_arm64_armv8-a_static_afdo-libgui_lto-thin/gen/aidl \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/common/aidl/${AIDL_COMMON}/gen/include \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/generated-headers/hardware/interfaces/media/1.0/android.hardware.media@1.0_genc++_headers/gen \
  -I$ROOT/include/v$SDK/arm64/include/system/libhwbinder/include \
  -I$ROOT/include/logging/liblog/include"

FLAGS="-std=c++17 -O2 -march=armv8-a -target aarch64-linux-android${SDK} -D__BIONIC__ -frtti -w -include $ROOT/include/compat.h"
LIBS="-L/system/lib64 -lgui -lui -lEGL -lGLESv2 -lbinder -lutils -llog -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-all"

# ── lib ───────────────────────────────────────────────────────────────────────
if [[ $BUILD_LIB -eq 1 ]]; then
    echo "[*] Building stub.so..."
    STUB_SRC="$OUT_LIBS/stub.cpp"
    cat > "$STUB_SRC" << 'STUB'
#include <stddef.h>
namespace android {
    class VectorImpl {
    public:
        virtual ~VectorImpl() {}
        virtual void do_construct(void*, size_t) const {}
        virtual void do_destroy(void*, size_t) const {}
        virtual void do_copy(void*, const void*, size_t) const {}
        virtual void do_splat(void*, const void*, size_t) const {}
        virtual void do_move_forward(void*, const void*, size_t) const {}
        virtual void do_move_backward(void*, const void*, size_t) const {}
    };
    class SortedVectorImpl : public VectorImpl {
    public:
        virtual ~SortedVectorImpl() {}
        virtual int do_compare(const void*, const void*) const { return 0; }
    };
}
void* __stub_force() {
    return new android::SortedVectorImpl();
}
STUB
    clang++ -shared -fPIC -frtti -target aarch64-linux-android${SDK} \
        -o "$OUT_LIBS/stub.so" "$STUB_SRC"

    STUB_OBJ="$OUT_LIBS/stub.o"
    clang++ -fPIC -frtti -target aarch64-linux-android${SDK} \
        -c "$STUB_SRC" -o "$STUB_OBJ"

    echo "[*] Building libstratum.so..."
    clang++ $FLAGS $INCLUDES -shared -fPIC \
        $ROOT/src/main.cpp "$STUB_OBJ" \
        $LIBS \
        -o "$OUT_LIBS/libstratum.so"
    rm "$STUB_SRC" "$STUB_OBJ"
fi

if [[ $BUILD_LIB -eq 1 && $BUILD_EXAMPLES -eq 1 ]]; then
    if [ ! -f "$DEVICE_DIR/app.cpp" ]; then
        echo "error: missing $DEVICE_DIR/app.cpp — copy src/default.cpp to get started"
        exit 1
    fi
    bash "$ROOT/scripts/build_app.sh" "$DEVICE" "$DEVICE_DIR/app.cpp" stratum_binary
fi

# ── examples ──────────────────────────────────────────────────────────────────
if [[ $BUILD_EXAMPLES -eq 1 ]]; then
    if [[ ${#ONLY[@]} -gt 0 ]]; then
        targets=()
        for name in "${ONLY[@]}"; do
            if [[ $name == *.cpp ]]; then
                [[ ! -f $name ]] && echo "[!] File not found: $name" && exit 1
                targets+=("$name")
            else
                f=""
                [[ -f $ROOT/apps/utils/$name.cpp ]] && f=$ROOT/apps/utils/$name.cpp
                [[ -f $ROOT/apps/demos/$name.cpp ]] && f=$ROOT/apps/demos/$name.cpp
                [[ -z $f ]] && echo "[!] App not found: $name" && exit 1
                targets+=("$f")
            fi
        done
    else
        targets=($ROOT/apps/utils/*.cpp $ROOT/apps/demos/*.cpp)
    fi

    mkdir -p "$DEVICE_DIR/out/extras"
    for f in "${targets[@]}"; do
        name=$(basename "$f" .cpp)
        bash "$ROOT/scripts/build_app.sh" "$DEVICE" "$f"
        mv "$OUT_BINS/$name" "$DEVICE_DIR/out/extras/$name" 2>/dev/null || true
    done
fi

# ── package module zip ────────────────────────────────────────────────────────
if [[ $BUILD_LIB -eq 1 && $BUILD_EXAMPLES -eq 1 ]]; then
    echo "[*] Packaging module zip..."

    STAGING="$DEVICE_DIR/out/.staging"
    rm -rf "$STAGING"
    mkdir -p "$STAGING/system/lib64" "$STAGING/system/bin"
    cp -r "$DEVICE_DIR/stratum-boot/." "$STAGING/"
    mkdir -p "$STAGING/system/lib64" "$STAGING/system/bin"

    cp "$OUT_LIBS/libstratum.so"  "$STAGING/system/lib64/libstratum.so"
    cp "$OUT_LIBS/stub.so"        "$STAGING/system/lib64/stub.so"
    cp "$OUT_BINS/stratum_binary" "$STAGING/system/bin/stratum_binary"

    MODULE_ZIP="$DEVICE_DIR/out/${DEVICE}-stratum-boot.zip"
    cd "$STAGING" && zip -r9 "$MODULE_ZIP" . > /dev/null
    cd "$ROOT"
    rm -rf "$STAGING"

    echo ""
    echo "[*] Done!"
    echo "    libs : $OUT_LIBS/"
    echo "    bins : $OUT_BINS/"
    echo "    zip  : $MODULE_ZIP"
fi
