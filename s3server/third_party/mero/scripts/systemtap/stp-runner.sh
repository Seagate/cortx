#!/bin/bash
set -eu

# Note: if you want to use systemtap with Mero kernel module (m0mero.ko)
# which is not installed in the system, you need to symlink m0mero.ko and
# run depmod. Example:
# # sudo ln -s /work/mero/mero/m0mero.ko \
#              /lib/modules/`uname -r`/kernel/drivers/m0mero.ko
# # sudo depmod
# It's useful when you want to debug a module which is in the source tree.

M0_SRC_DIR="$(readlink -f $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*/*}"
. $M0_SRC_DIR/utils/functions  # die

if [ ${0##*/} = $(basename $(readlink -f $0)) ]; then
	die "${0##*/}: Don't execute this script, use a dedicated symlink."
fi

STP_SCRIPT=$0.stp
[ -r "$STP_SCRIPT" ] || die "$STP_SCRIPT: No such file"

LIBMERO="$M0_SRC_DIR/mero/.libs/libmero.so"
M0D="$M0_SRC_DIR/mero/.libs/lt-m0d"
UT="$M0_SRC_DIR/ut/.libs/lt-m0ut"
M0TAPSET="$M0_SRC_DIR/scripts/systemtap/tapset"

set -x
stap -vv -d "$LIBMERO" -d "$M0D" -d "$UT" --ldd                         \
	-DMAXTRACE=10 -DSTP_NO_OVERLOAD                                 \
	-DMAXSKIPPED=1000000 -DMAXERRORS=1000 -DMAXSTRINGLEN=2048       \
	-t -DTRYLOCKDELAY=5000                                          \
	-I "$M0TAPSET" "$STP_SCRIPT" "$LIBMERO" "$M0D"
