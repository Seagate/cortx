#!/usr/bin/env bash
#
# Tests the fwait feature on Mero.
#

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh

fwait_test()
{
	`dirname $0`/m0t1fs_fwait_test_helper $MERO_M0T1FS_MOUNT_DIR
	return $?
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	echo "About to start Mero service"
	local multiple_pools=0
	mero_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
		return 1
	fi

	rc=0
	echo "mero service started"

	mkdir -p $MERO_M0T1FS_MOUNT_DIR
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR "oostore" || return 1

	fwait_test || {
		echo "Failed: Fwait test failed.."
		rc=1
	}

	unmount_m0t1fs $MERO_M0T1FS_MOUNT_DIR &>> $MERO_TEST_LOGFILE

	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		return 1
	fi

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MERO_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit fwait $?
