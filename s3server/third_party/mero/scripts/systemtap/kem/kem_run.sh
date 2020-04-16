#!/bin/bash

# Before kem_run.sh script is executed, please
# run make to build kemd.ko kernel module and
# kemc user application:
#
# $ make

KEM_DIR="$(readlink -f $0)"
KEM_DIR="${KEM_DIR%/*}"

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit
fi

echo Inserting kemd.ko
insmod $KEM_DIR/kemd.ko

for i in $(seq 0 $(($(nproc)-1)))
do
    mknod /dev/kemd$i c 60 $i
done

echo Running Systemtap
stap -g $KEM_DIR/kemd.stp &
stapPID=$!

# Wait stap for start
sleep 20

echo Running KEM clients
for i in $(seq 0 $(($(nproc)-1)))
do
    $KEM_DIR/m0kemc $i > kemc_cpu$i.log 2>&1 &
    kemcPIDs[$i]=$!
done

echo Collecting kernel events...
sleep 20

echo Shutdown Systemtap
kill -s SIGTERM $stapPID
sleep 5

for pid in "${kemcPIDs[@]}"
do
    kill -s SIGINT $pid
done
sleep 2

for i in $(seq 0 $(($(nproc)-1)))
do
    rm -f /dev/kemd$i
done

echo Removing kemd.ko
rmmod $KEM_DIR/kemd.ko

for i in $(seq 0 $(($(nproc)-1)))
do
    $KEM_DIR/../../../utils/m0run m0addb2dump $PWD/_kemc$i/o/100000000000000:2 | grep pagefault -A 1 | head -n 40
    $KEM_DIR/../../../utils/m0run m0addb2dump $PWD/_kemc$i/o/100000000000000:2 | grep ctx_switch -A 1 | head -n 40
done
