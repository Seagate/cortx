#!/usr/bin/env bash
# see original file at utils/linux_kernel/ut.sh

# Small wrapper to run mero network benchmark node module

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root"
    exit 1
fi

d="`git rev-parse --show-cdup`"
if [ -n "$d" ]; then
    cd "$d"
fi

. m0t1fs/linux_kernel/st/common.sh

MODLIST="m0mero.ko"
MODMAIN="m0nettestd.ko"

log='/var/log/kern'
if [ ! -e "$log" ]; then
    log='/var/log/messages'
fi
tailseek=$(( $(stat -c %s "$log") + 1 ))

# currently, kernel UT runs as part of loading m0ut module
modload_m0gf
modload
insmod $MODMAIN $*
rmmod $MODMAIN
modunload
modunload_m0gf

tail -c+$tailseek "$log" | grep ' kernel: '
