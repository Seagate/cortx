#!/usr/bin/env bash
set -e

[[ $UID -eq 0 ]] || {
    echo 'Must be run by superuser' >&2
    exit 1
}

M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. $M0_SRC_DIR/utils/functions # report_and_exit

cd $M0_SRC_DIR

echo 'Installing Mero services'
scripts/install-mero-service -u

rm -rf /etc/mero
rm -f  /etc/sysconfig/mero
scripts/install-mero-service -l

PATH=$PATH:$M0_SRC_DIR/utils
sh ./scripts/install/opt/seagate/eos/core/sanity/eos_core_sanity.sh
rc=$?

scripts/install-mero-service -u
report_and_exit sanity $rc
