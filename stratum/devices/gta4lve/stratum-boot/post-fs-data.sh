#!/system/bin/sh
mkdir -p /data/local/tmp/stratum
LOG=/data/local/tmp/stratum/boot.log
> $LOG
echo "$(date) post-fs-data started" >> $LOG

# stage 1 - waiting
echo 100 > /sys/class/timed_output/vibrator/enable

# wait for surfaceflinger
(
    until pidof surfaceflinger > /dev/null 2>&1; do
        sleep 0.2
    done
    echo "$(date) surfaceflinger up, launching stratum" >> $LOG
    /system/bin/stratum >> $LOG 2>&1
) &

echo "$(date) done" >> $LOG
