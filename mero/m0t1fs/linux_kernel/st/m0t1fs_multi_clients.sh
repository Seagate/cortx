#!/usr/bin/env bash

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh

multi_clients()
{
	NODE_UUID=`uuidgen`
	local multiple_pools=0

	mero_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
		return 1
	fi

	multi_client_test
	rc=$?

	# mero_service stop --collect-addb
	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		return 1
	fi

	if [ $rc -ne "0" ]
	then
		echo "Failed m0t1fs multi-clients tests: rc=$rc"
		return $rc
	fi
}

main()
{
	sandbox_init

	echo "Starting multi clients testing:"
	echo "Test log will be stored in $MERO_TEST_LOGFILE."

	multi_clients 2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=${PIPESTATUS[0]}

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MERO_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit multi-client $?
