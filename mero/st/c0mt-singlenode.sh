#!/usr/bin/env bash
set -exu


M0_SRC_DIR="$(readlink -f $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*}"
export H0_SRC_DIR=${H0_SRC_DIR:-${M0_SRC_DIR%/*}/halon}


. $M0_SRC_DIR/utils/functions

# Start halon and all involved mero services.
$H0_SRC_DIR/scripts/h0 start
[ $? -eq 0 ] || report_and_exit c0mt $?

# Run clovis load test with parameters $CLOVIS_EP, $HALON_EP, $PROFILE, $PROCESS
PROFILE=$(halonctl mero status | grep profile | sed -r 's/[ \t]+//g' | sed -r 's/profile:(0x[0-9a-z]+:0x[0-9a-z]+)/\1/g')
PROCESS=$(halonctl mero status | grep clovis-app | grep N/A | head -1 | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~clovis-app/\1/g')
CLOVIS_EP=$(halonctl mero status | grep clovis-app | grep N/A | head -1 | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~clovis-app/\2/g')
HALON_EP=$(halonctl mero status | grep halon  | sed -r 's/[ \t]+/~/g' | sed -r 's/.*~(0x[0-9a-z]+:0x[0-9a-z]+)~(.*)~halon/\2/g')
$M0_SRC_DIR/clovis/st/mt/utils/c0mt -l $CLOVIS_EP -h $HALON_EP \
				    -p $PROFILE -f $PROCESS
[ $? -eq 0 ] || report_and_exit c0mt $?

# Stop halon and all involved mero services.
$H0_SRC_DIR/scripts/h0 stop
report_and_exit c0mt $?
