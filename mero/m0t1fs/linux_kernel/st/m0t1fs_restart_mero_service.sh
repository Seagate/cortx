#!/usr/bin/env bash
set +e

## CAUTION: This path will be removed by superuser.
SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.restart_mero_service}

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh

rcancel_sandbox="$MERO_M0T1FS_TEST_DIR/rcancel_sandbox"
source_file="$rcancel_sandbox/rcancel_source"

rcancel_mero_service_start()
{
	local multiple_pools=${1:-0}

	mero_service start $multiple_pools
	if [ $? -ne 0 ]
	then
		echo "Failed to start Mero Service"
		return 1
	fi
	echo "mero service started"
}

main()
{
	NODE_UUID=`uuidgen`
	local rc

	sandbox_init

	echo "*********************************************************"
	echo "Start: 1. Mero service start-stop"
	echo "*********************************************************"
	rcancel_mero_service_start || return 1
	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		if [ $rc -eq "0" ]; then
			return 1
		fi
	fi

	echo "Call unprepare"
	unprepare
	echo "Done with unprepare"

	echo "*********************************************************"
	echo "End: 1. Mero service start-stop"
	echo "*********************************************************"

	echo "pgrep m0"
	pgrep m0
	echo "ps -aef | grep m0"
	ps -aef | grep m0

	echo "lsmod | grep m0"
	lsmod | grep m0
	echo "lsmod | grep lnet"
	lsmod | grep lnet

	echo "*********************************************************"
	echo "Start: 2. Mero service start-stop"
	echo "*********************************************************"

	rcancel_mero_service_start 1 || return 1
	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		if [ $rc -eq "0" ]; then
			return 1
		fi
	fi
	echo "*********************************************************"
	echo "End: 2. Mero service start-stop"
	echo "*********************************************************"

	echo "Test log available at $MERO_TEST_LOGFILE."
}

trap unprepare EXIT
main
