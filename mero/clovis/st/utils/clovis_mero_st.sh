#!/bin/bash

# Script for starting Clovis system tests in "scripts/m0 run-st"

#set -x

clovis_st_util_dir=`dirname $0`
m0t1fs_st_dir=$clovis_st_util_dir/../../../m0t1fs/linux_kernel/st

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_st_dir/common.sh
. $m0t1fs_st_dir/m0t1fs_common_inc.sh
. $m0t1fs_st_dir/m0t1fs_client_inc.sh
. $m0t1fs_st_dir/m0t1fs_server_inc.sh
. $m0t1fs_st_dir/m0t1fs_sns_common_inc.sh

# Import wrapper for Clovis ST framework which can be run with other
# mero tests or can be used separately.
. $clovis_st_util_dir/clovis_local_conf.sh
. $clovis_st_util_dir/clovis_st_inc.sh

# Set the mode of Clovis [user|kernel]
umod=1

clovis_st_run_tests()
{
	# Start the tests
	if [ $umod -eq 1 ]; then
		clovis_st_start_u
		clovis_st_stop_u
	else
		clovis_st_start_k
		clovis_st_stop_k
	fi
}

clovis_st_set_failed_dev()
{
	disk_state_set "failed" $1 || {
		echo "Failed: pool_mach_set_failure..."
		return 1
	}
	disk_state_get $1
}

clovis_st_dgmode()
{
	NODE_UUID=`uuidgen`
	multiple_pools_flag=0
	mero_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
		return 1
	fi

	#local mountopt="oostore,verify"
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1
	# Inject failure to device 1
	fail_device=1
	clovis_st_set_failed_dev $fail_device || {
		return 1
	}

	unmount_and_clean &>> $MERO_TEST_LOGFILE
	# Run tests
	clovis_st_run_tests
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
		echo "Failed m0t1fs system tests."
		return 1
	fi

	echo "System tests status: SUCCESS."

}

clovis_st()
{
	NODE_UUID=`uuidgen`
	multiple_pools_flag=0
	mero_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
		return 1
	fi

	clovis_st_run_tests
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
		echo "Failed Clovis system tests."
		return 1
	fi

	echo "System tests status: SUCCESS."
}

main()
{
	local rc

	echo "Clovis system tests start:"
	echo "Test log will be stored in $MERO_TEST_LOGFILE."

	set -o pipefail

	umod=1
	echo -n "Start Clovis Tests [User Mode] ... "
	clovis_st $umod -2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo -n "Start Clovis Degraded mode Tests [User Mode] ... "
	clovis_st_dgmode $umod 2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=$?
	echo "Done"

	umod=0
	echo -n "Start Clovis Tests [Kernel Mode] ... "
	clovis_st $umod -2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo -n "Start Clovis Degraded mode Tests [Kernel Mode] ... "
	clovis_st_dgmode $umod 2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo "Test log available at $MERO_TEST_LOGFILE."

	return $rc
}

trap unprepare EXIT
main


# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
