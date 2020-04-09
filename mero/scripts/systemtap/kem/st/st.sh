#!/bin/bash
set -eu

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.51kem}

M0_SRC_DIR="$(readlink -f $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*/*/*/*}"

. "$M0_SRC_DIR/utils/functions" # sandbox_init, report_and_exit

sandbox_init
rc=0;
(cd "$M0_SRC_DIR/scripts/systemtap/kem" && make)
"$M0_SRC_DIR/scripts/systemtap/kem/kem_run.sh" || rc=$?
(cd "$M0_SRC_DIR/scripts/systemtap/kem" && make clean)
sandbox_fini $rc
report_and_exit kem_run $rc
