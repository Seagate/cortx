#!/usr/bin/env bash
set -eu

# Next lines are useful for ST scripts debugging
# set -eux
# export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.net-st}

CWD=$(dirname $(readlink -f $0))
M0_SRC_DIR=${CWD%/*/*/*}

. $M0_SRC_DIR/utils/functions # die, opcode, sandbox_init, report_and_exit
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $CWD/st-config.sh

role_space()
{
	local role=$1
	[ "$role" == "$KERNEL_ROLE" ] && echo -n "kernel space"
	[ "$role" != "$KERNEL_ROLE" ] && echo -n "user space"
}

unload_all() {
	modunload
	modunload_m0gf
}
trap unload_all EXIT

modprobe_lnet
lctl network up > /dev/null
modload_m0gf
modload || exit $?

sandbox_init
export TEST_RUN_TIME=5
echo "transfer machines endpoint prefix is $LNET_PREFIX"
for KERNEL_ROLE in "none" "client" "server"; do
	export KERNEL_ROLE
	echo -n "------ test client in $(role_space client), "
	echo "test server in $(role_space server)"
	echo "--- ping test (test message size is 4KiB)"
	sh $CWD/st-ping.sh
	echo "--- bulk test (test message size is 1MiB)"
	sh $CWD/st-bulk.sh
done
sandbox_fini
report_and_exit net 0
