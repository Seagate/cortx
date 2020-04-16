#!/usr/bin/env bash
set -eu

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.rpc-st}

M0_SRC_DIR="$(readlink -f $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*/*}"
[ -n "${SUDO:-}" ] || SUDO='sudo -E'

. "$M0_SRC_DIR/utils/functions" # sandbox_init, report_and_exit

rc=0
sandbox_init
$SUDO "$M0_SRC_DIR/conf/st" insmod
"$M0_SRC_DIR/rpc/it/st" || rc=$?
$SUDO "$M0_SRC_DIR/conf/st" rmmod
sandbox_fini $rc
report_and_exit rpcping $rc
