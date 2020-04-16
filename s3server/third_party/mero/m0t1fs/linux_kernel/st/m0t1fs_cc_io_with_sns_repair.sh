#!/usr/bin/env bash

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh

###################################################
# SNS repair is only supported in COPYTOOL mode,
# because ios need to hash gfid to mds. In COPYTOOL
# mode, filename is the string format of gfid.
###################################################

# Files used for:
# - SNS Repair concurrent with IO testing
# - SNS Repair concurrent with delete testing
files=(
	10000
	10001
	10002
	10003
	10004
	10005
	10006
	10007
	10008
	10009
	10010
	10011
	10012
	10013
	10014
	10015
	10016
	10017
	10018
	10019
)

bs=(
	32
	32
	32
	32
	32
	32
	32
	32
	32
	10240
	32
	32
	32
	32
	32
	32
	32
	32
	32
	1024
)

file_size=(
	50
	70
	30
	0
	40
	0
	60
	90
	10
	15
	5
	80
	50
	120
	100
	0
	150
	80
	130
	90
)

CC_REPAIR_READ_CONTAINER=111:
CC_REPAIR_WRITE_CONTAINER=999:
CC_REPAIR_DELETE_CONTAINER=222:
CC_REBAL_DELETE_CONTAINER=333:

CC_REPAIR_READ_FILES_NR=12
CC_REPAIR_WRITE_FILES_BATCH1_NR=4
CC_REPAIR_WRITE_FILES_BATCH2_NR=8
CC_REPAIR_DELETE_FILES_NR=${#files[*]}
CC_REBAL_DELETE_FILES_NR=${#files[*]}

N=3
K=3
P=15
stride=64
src_bs=10M
src_count=17
fail_device1=1
fail_device2=9
fail_device3=3

verify()
{
	local FILE=$1
	local BS=$2
	local COUNT=$3

	local_read $BS $COUNT || return $?
	read_and_verify $FILE $BS $COUNT || return $?

	echo "$FILE verification sucess"
}

verify_all()
{
	local container=$1
	local start=$2
	local end=$3

        for ((i=$start; i < $end; i++)) ; do
		verify "$container${files[$i]}" $((${bs[$i]} * 1024))  ${file_size[$i]} || return $?
        done
}

delete_all()
{
	local container=$1
	local start=$2
	local end=$3

	echo "start $start, end $end"
	for ((i=$start; i < $end; i++)) ; do
		echo "rm -f $MERO_M0T1FS_MOUNT_DIR/$container${files[$i]} &"
		rm -f $MERO_M0T1FS_MOUNT_DIR/$container${files[$i]} &
	done
	sleep 5
}

ls_all()
{
	local container=$1
	local start=$2
	local end=$3
	local expected_rc=$4

	echo "start $start, end $end"
	for ((i=$start; i < $end; i++)) ; do
		ls -lh $MERO_M0T1FS_MOUNT_DIR/$container${files[$i]}
		rc=$?
		if [ $rc -ne $4 ]; then
			echo "Error: ls -lh $MERO_M0T1FS_MOUNT_DIR/$container${files[$i]}: rc $rc"
			return 1 # $rc may be 0 if $expected_rc is 0
		fi
        done
}


create_files_and_checksum()
{
	local container=$1
	local start=$2
	local end=$3

	# With unit size of 32K dd fails for the file "1009".
	# It runs with unit size 64K. A jira MERO-1086 tracks this issue.
	for ((i=$start; i < $end; i++)) ; do
		touch_file $MERO_M0T1FS_MOUNT_DIR/$container${files[$i]} $stride
		_dd $container${files[$i]} $((${bs[$i]} * 1024)) ${file_size[$i]}
		verify $container${files[$i]} $((${bs[$i]} * 1024)) ${file_size[$i]}
	done
}

sns_repair_rebal_cc_io_test()
{
	local container=$CC_REPAIR_READ_CONTAINER
	local files_nr=$CC_REPAIR_READ_FILES_NR
	local rc=0


	echo "*********************************************************"
	echo "TC-1 Start: SNS Repair concurrent with IO testing"
	echo "*********************************************************"

	echo "**** List all the files before setting device failure. ****"
	ls_all $container 0 $files_nr 0 || return $?

	echo "*** Set device Failure ***"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "Device $fail_device1 and $fail_device2 failed. Do dgmode read"
	verify_all $container 0 $files_nr || return $?

	echo "*** Query device state ***"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "*** Start sns repair and it will run in background ****"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair
	sleep 5
	#echo "**** Create files while sns repair is in-progress ****"
	#create_files_and_checksum $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH1_NR

	echo **** Perform read during repair. ****
	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH1_NR || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	echo "SNS Repair done."

	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH1_NR || return $?

	echo "*** Query device state ***"
	disk_state_get $fail_device1 $fail_device2 || return $?

        echo "Starting SNS Re-balance.."
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	sns_rebalance || return $?

	wait_for_sns_repair_or_rebalance "rebalance" || return $?
	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Rebalance done."

	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH1_NR || return $?

	disk_state_set "failed" $fail_device3 || return $?

	echo "**** Start sns repair and it will run in background ****"
	disk_state_set "repair" $fail_device3 || return $?
	sns_repair
	sleep 5
	#echo "**** Create files while sns repair is in-progress ****"
	#create_files_and_checksum $CC_REPAIR_WRITE_CONTAINER $CC_REPAIR_WRITE_FILES_BATCH1_NR $CC_REPAIR_WRITE_FILES_BATCH2_NR

	echo **** Perform read during repair. ****
	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH2_NR || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?
	disk_state_set "repaired" $fail_device3 || return $?

	echo "SNS Repair done."
	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH2_NR || return $?

	echo "*** Query device state ***"
	disk_state_get $fail_device3 || return $?

	verify_all $container 0 $files_nr || return $?

	echo "Starting SNS Re-balance and it will run in background..."
	disk_state_set "rebalance" $fail_device3 || return $?
	sns_rebalance || return $?

	echo "Perform read during SNS Rebalance."
	verify_all $container 0 $files_nr || return $?
	#verify_all $CC_REPAIR_WRITE_CONTAINER 0 $CC_REPAIR_WRITE_FILES_BATCH2_NR || return $?

	echo "wait for SNS Re-balance "
	wait_for_sns_repair_or_rebalance "rebalance" || return $?
	disk_state_set "online" $fail_device3 || return $?
	echo "SNS Rebalance done."

	echo "*** Query device state ***"
	disk_state_get $fail_device1 $fail_device2 $fail_device3

	verify_all $container 0 $files_nr || return $?

	echo "*********************************************************"
	echo "TC-1 End: SNS Repair concurrent with IO testing"
	echo "*********************************************************"
	return 0
}

sns_repair_cc_delete_test()
{
	local container=$CC_REPAIR_DELETE_CONTAINER
	local files_nr=$CC_REPAIR_DELETE_FILES_NR
	local rc=0

	echo "*********************************************************"
	echo "TC-2 Start: SNS Repair concurrent with delete testing"
	echo "*********************************************************"

	verify_all $container 0 $files_nr || return $?

	echo "*** Set device Failure ***"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "*** Query device state ***"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "**** List all the files before repair. ****"
	ls_all $container 0 $files_nr 0 || return $?

	echo "*** Start sns repair and it will run in background ****"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair
	sleep 5

	echo -e "**** Perform delete during repair. ****"
	delete_all $container 0 $files_nr

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	echo "SNS Repair done."

	sns_repair_or_rebalance_status "repair" || return $?

	echo "**** Rebalance the failed devices before starting next test ****"
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	sns_rebalance || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Rebalance done."

	echo "fsync before verifying that all the files are deleted"
	$M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_fsync_test_helper $MERO_M0T1FS_MOUNT_DIR
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "fsync failed"
		return 1
	fi

	ps aux | grep $MERO_M0T1FS_MOUNT_DIR | grep -v "grep"
	rm_count=`ps aux | grep $MERO_M0T1FS_MOUNT_DIR | grep -v "grep" -c`
	echo "rm_count : $rm_count"
	if [ $rm_count -ne 0 ]; then
		echo "Error: $rm_count rm jobs still running...."
		return 1
	fi

	echo -e "\n**** List all the files after concurrent repair and delete (shall not be found) ****\n"
	ls_all $container 0 $files_nr 2 || return $?

	echo "*********************************************************"
	echo "TC-2 End: SNS Repair concurrent with delete testing"
	echo "*********************************************************"

	return 0
}

sns_rebalance_cc_delete_test()
{
	local container=$CC_REBAL_DELETE_CONTAINER
	local files_nr=$CC_REBAL_DELETE_FILES_NR
	local rc=0

	echo "*********************************************************"
	echo "TC-3 Start: SNS Rebalance concurrent with delete testing"
	echo "*********************************************************"

	echo "*** Set device Failure ***"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "*** Query device state ***"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "*** Start sns repair and it will run in background ****"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair
	sleep 5

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	echo "SNS Repair done."

	sns_repair_or_rebalance_status "repair" || return $?

	verify_all $container 0 $files_nr || return $?

	echo "**** List all the files before rebalance. ****"
	ls_all $container 0 $files_nr 0 || return $?

	echo "*** Start sns rebalance and it will run in background ****"
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	sns_rebalance || return $?

	echo -e "**** Perform delete during rebalance. ****"
	delete_all $container 0 $files_nr

	echo "wait for SNS Re-balance"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?
	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Rebalance done."

	sns_repair_or_rebalance_status "rebalance" || return $?

	echo "fsync before verifying that all the files are deleted"
	$M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_fsync_test_helper $MERO_M0T1FS_MOUNT_DIR
	rc=$?
	if [ $rc -ne 0 ]; then
		echo "fsync failed"
		return 1
	fi

	ps aux | grep $MERO_M0T1FS_MOUNT_DIR| grep -v "grep"
	rm_count=`ps aux | grep $MERO_M0T1FS_MOUNT_DIR| grep -v "grep" -c`
	echo "rm_count : $rm_count"
	if [ $rm_count -ne 0 ]; then
		"Error: $rm_count rm jobs still running...."
		return 1
	fi

	echo -e "\n**** List all the files after concurrent rebalance and delete (shall not be found) ****\n"
	ls_all $container 0 $files_nr 2 || return $?

	echo "*********************************************************"
	echo "TC-3 End: SNS Rebalance concurrent with delete testing"
	echo "*********************************************************"

	return 0
}

create_files()
{
	echo "*** Creating local source file ***"
	local_write $src_bs $src_count || rc=$?

	echo -e "\n*** Creating files for 'repair/rebalance concurrent with IO' test ***"
	create_files_and_checksum $CC_REPAIR_READ_CONTAINER 0 $CC_REPAIR_READ_FILES_NR

	echo -e "\n*** Creating files for 'repair concurrent with delete' test ***"
	create_files_and_checksum $CC_REPAIR_DELETE_CONTAINER 0 $CC_REPAIR_DELETE_FILES_NR

	echo -e "\n*** Creating files for 'rebalance concurrent with delete' test ***"
	create_files_and_checksum $CC_REBAL_DELETE_CONTAINER 0 $CC_REBAL_DELETE_FILES_NR
}

main()
{
	local rc=0

	NODE_UUID=`uuidgen`
	local multiple_pools=0

	sandbox_init

	mero_service start $multiple_pools  $stride $N $K $P || {
		echo "Failed to start Mero Service."
		return 1
	}

	sns_repair_mount || rc=$?

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	create_files

	if [[ $rc -eq 0 ]] && ! sns_repair_rebal_cc_io_test ; then
		echo "Failed: SNS repair concurrent with IO failed.."
		rc=1
	fi

	if [[ $rc -eq 0 ]] && ! sns_repair_cc_delete_test ; then
		echo "Failed: SNS repair concurrent with delete failed.."
		rc=1
	fi

	if [[ $rc -eq 0 ]] && ! sns_rebalance_cc_delete_test ; then
		echo "Failed: SNS rebalance concurrent with delete failed.."
		rc=1
	fi

	echo "unmounting and cleaning.."
	unmount_and_clean &>> $MERO_TEST_LOGFILE

	mero_service stop || {
		echo "Failed to stop Mero Service."
		rc=1
	}

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MERO_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit sns-cc $?
