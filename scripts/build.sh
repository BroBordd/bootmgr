#!/bin/bash
set -e

ROOT=$(realpath $(dirname $0)/..)
STRATUM="$ROOT/stratum"
DEVICE=$1

if [ -z "$DEVICE" ]; then
    echo "usage: $0 <device> [-m|--manager-only] [-e|--entries-only]"
    exit 1
fi

if [ ! -f "$STRATUM/scripts/build.sh" ]; then
    echo "error: stratum submodule not initialized"
    echo "run:   git submodule update --init --recursive"
    exit 1
fi

DEVICE_DIR="$STRATUM/devices/$DEVICE"
OUT_BINS="$DEVICE_DIR/out/bins"
OUT_LIBS="$DEVICE_DIR/out/libs"
OUT_ENTRIES="$DEVICE_DIR/out/entries"

if [ ! -d "$DEVICE_DIR" ]; then
    echo "error: '$DEVICE_DIR' not found"
    exit 1
fi

BUILD_MGR=1
BUILD_ENTRIES=1
shift
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--manager-only) BUILD_ENTRIES=0; shift ;;
        -e|--entries-only) BUILD_MGR=0;     shift ;;
        *) shift ;;
    esac
done

# ── autobuild stratum if needed ───────────────────────────────────────────────
if [ ! -f "$OUT_LIBS/libstratum.so" ]; then
    echo "[*] libstratum.so not found, building stratum..."
    bash "$STRATUM/scripts/build.sh" "$DEVICE" -l || true
fi

# ── bootmgr ───────────────────────────────────────────────────────────────────
if [ $BUILD_MGR -eq 1 ]; then
    bash "$STRATUM/scripts/build_app.sh" "$DEVICE" "$ROOT/src/bootmgr.cpp" bootmgr
fi

# ── entries ───────────────────────────────────────────────────────────────────
if [ $BUILD_ENTRIES -eq 1 ]; then
    mkdir -p "$OUT_ENTRIES"
    # Build every .cpp file found in src/entries/
    for f in "$ROOT/src/entries/"*.cpp; do
        if [ -f "$f" ]; then
            name=$(basename "$f" .cpp)
            bash "$STRATUM/scripts/build_app.sh" "$DEVICE" "$f"
            mv "$OUT_BINS/$name" "$OUT_ENTRIES/$name"
        fi
    done
fi

# ── package zip ───────────────────────────────────────────────────────────────
echo "[*] Packaging module zip..."

STAGING="$ROOT/.staging"
rm -rf "$STAGING"
mkdir -p "$STAGING/system/lib64" "$STAGING/system/bin" "$STAGING/entries"
cp -r "$ROOT/module/." "$STAGING/"

cp "$OUT_LIBS/libstratum.so" "$STAGING/system/lib64/libstratum.so"
cp "$OUT_LIBS/stub.so"       "$STAGING/system/lib64/stub.so"
cp "$OUT_BINS/bootmgr"       "$STAGING/system/bin/bootmgr"

# Copy all compiled entries into the zip
if [ -d "$OUT_ENTRIES" ]; then
    cp "$OUT_ENTRIES/"* "$STAGING/entries/" 2>/dev/null || true
fi

ZIP="$STRATUM/devices/$DEVICE/out/${DEVICE}-bootmgr.zip"
cd "$STAGING" && zip -r9 "$ZIP" . > /dev/null
cd "$ROOT"
rm -rf "$STAGING"

echo ""
echo "[*] Done!"
echo "    zip : $ZIP"
