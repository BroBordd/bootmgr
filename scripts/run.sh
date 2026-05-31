#!/bin/bash
ROOT=$(realpath $(dirname "$0")/..)
STRATUM="$ROOT/stratum"

if [ ! -f "$STRATUM/scripts/build.sh" ]; then
    echo "error: stratum submodule not initialized"
    echo "run:   git submodule update --init --recursive"
    exit 1
fi

if [ -z "$1" ]; then
    echo "usage: $0 <device>"
    exit 1
fi

DEVICE=$1
DEVICE_DIR="$STRATUM/devices/$DEVICE"
OUT_BINS="$DEVICE_DIR/out/bins"
OUT_LIBS="$DEVICE_DIR/out/libs"
OUT_ENTRIES="$DEVICE_DIR/out/entries"

if [ ! -d "$DEVICE_DIR" ]; then
    echo "error: '$DEVICE_DIR' not found"
    exit 1
fi

su -c "BOOTMGR_ENTRIES=$OUT_ENTRIES \
       LIB_DIR=$OUT_LIBS \
       LD_PRELOAD=$OUT_LIBS/stub.so \
       LD_LIBRARY_PATH=$OUT_LIBS:/system/lib64:/vendor/lib64 \
       $OUT_BINS/bootmgr"
