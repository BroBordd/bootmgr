#!/system/bin/sh
# Use Magisk's automatic module directory path instead of hardcoding
MODDIR=${0%/*}

mkdir -p /data/local/tmp/stratum
LOG=/data/local/tmp/stratum/bootmgr.log
> $LOG
echo "$(date) post-fs-data started" >> $LOG

echo 100 > /sys/class/timed_output/vibrator/enable

# Fix: Added /vendor/lib64 so Stratum can load the hardware graphics drivers
export LD_LIBRARY_PATH=$MODDIR/system/lib64:/system/lib64:/vendor/lib64
export LD_PRELOAD=$MODDIR/system/lib64/stub.so

# Ensure both the main manager and the OS entries are executable!
chmod +x $MODDIR/system/bin/bootmgr
chmod +x $MODDIR/entries/*

# wait for surfaceflinger
(
    until pidof surfaceflinger > /dev/null 2>&1; do
        sleep 0.2
    done
    echo "$(date) surfaceflinger up, launching stratum" >> $LOG
    
    # Run from the correct dynamic path
    $MODDIR/system/bin/bootmgr >> $LOG 2>&1
) &

echo "$(date) done" >> $LOG
