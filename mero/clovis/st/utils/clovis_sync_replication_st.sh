#!/bin/bash

#set -x

clovis_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )
m0t1fs_dir="$clovis_st_util_dir/../../../m0t1fs/linux_kernel/st"

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $m0t1fs_dir/m0t1fs_sns_common_inc.sh

. $clovis_st_util_dir/clovis_local_conf.sh
. $clovis_st_util_dir/clovis_st_inc.sh

N=1
K=2
P=15
stride=32
BLOCKSIZE=""
BLOCKCOUNT=""
OBJ_ID1="1048577"
OBJ_ID2="1048578"
OBJ_ID3="1048579"
# The second half is hex representations of OBJ_ID1, OBJ_ID2, and OBJ_ID3.
OBJ_HID1="0:100001"
OBJ_HID2="0:100002"
OBJ_HID3="0:100003"
PVER_1="7600000000000001:a"
PVER_2="7680000000000000:4"
PVER_3="7680000000000001:42"
clovis_pids=""
export cnt=1

# Dgmode IO

CLOVIS_TEST_DIR=$SANDBOX_DIR
CLOVIS_TEST_LOGFILE=$SANDBOX_DIR/clovis_`date +"%Y-%m-%d_%T"`.log
CLOVIS_TRACE_DIR=$SANDBOX_DIR/clovis

parity_verify_enabled="true"
parity_verify_disabled="false"
# Takes as an input the disk to be failed.
degraded_read()
{
	fail_device=$1
	parity_verify=$2

	# fail a disk and read an object
	disk_state_set "failed" $fail_device || {
		echo "Operation to mark device failure failed."
		error_handling 1
	}

	# Test degraded read
	rm -f $dest_file
	echo "Conducting a degraded read with failed device being $fail_device"
	io_conduct "READ" $OBJ_ID1 $dest_file $parity_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Obj read in degraded mode differs."
		error_handling $rc
	fi
	rm -f $dest_file
}

# Takes as an input the number of failures occurred so fat at the disk level.
# Reads the object from newly created pool version.
# Checks that a new pool version indeed gets assigned at the face of disk
# failure
post_failure_pver_check()
{
	fail_count=$1

	echo "Check pver of the first object"
	output=`getfattr -n pver $MERO_M0T1FS_MOUNT_DIR/"$OBJ_HID1"`
	echo $output
	if [[ $output != *"$PVER_1"* ]]
	then
		echo "getattr failed on $OBJ_HID1."
		error_handling 1
	fi
	echo "Check pver of the second object, created post device failure."
	output=`getfattr -n pver $MERO_M0T1FS_MOUNT_DIR/"$OBJ_HID2"`
	echo $output
	if [[ $output != *"$PVER_2"* ]]
	then
		echo "getattr failed on $OBJ_HID2 when total failures are
		      $fail_count"
		error_handling 1
	fi

	# Read a file from new pool version.
	rm -f $dest_file
	io_conduct "READ" $OBJ_ID2 $dest_file false
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Reading a file from a new pool version failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "File read from a new pool version differs."
		error_handling $rc
	fi

	if [ $fail_count -eq 2 ]
	then
		rm -f $dest_file
		io_conduct "READ" $OBJ_ID3 $dest_file false
		rc=$?
		if [ $rc -ne "0" ]
		then
			echo "Reading a file from a new pool version failed."
			error_handling $rc
		fi
		diff $src_file $dest_file
		rc=$?
		if [ $rc -ne "0" ]
		then
			echo "File read from a new pool version differs."
			error_handling $rc
		fi
		echo "Check pver of the third object"
		output=`getfattr -n pver $MERO_M0T1FS_MOUNT_DIR/"$OBJ_HID3"`
		echo $output
		if [[ $output != *"$PVER_3"* ]]
		then
			echo "getattr failed on $OBJ_HID3 when failures are
			      $fail_count"
			error_handling 1
		fi
	fi
}

main()
{

	sandbox_init

	NODE_UUID=`uuidgen`
	clovis_dgmode_sandbox="$CLOVIS_TEST_DIR/sandbox"
	src_file="$CLOVIS_TEST_DIR/clovis_source"
	dest_file="$CLOVIS_TEST_DIR/clovis_dest"
	rc=0

	dd if=/dev/urandom bs=4K count=100 of=$src_file 2> $CLOVIS_TEST_LOGFILE || {
		echo "Failed to create a source file"
		unmount_and_clean &>>$MERO_TEST_LOGFILE
		mero_service_stop
		return 1
	}
	mkdir $CLOVIS_TRACE_DIR

	mero_service_start $N $K $P $stride

	#Initialise dix
	dix_init
	#mount m0t1fs as well. This helps in two ways:
	# 1) Currently clovis does not have a utility to check attributes of an
	#    object. Hence checking of attributes is done by fetching them via
	#    m0t1fs.
	# 2) A method to send HA notifications assumes presence of m0t1fs. One
	#    way to circumvent this is by assigning same end-point to clovis,
	#    but creating a clovis instance concurrently with HA notifications
	#    is hard. Another way is to re-write the method to send HA
	#    notifications by excluding m0t1fs end-point. We have differed these
	#    changes in current patch.
	local mountopt="oostore,verify"
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1
	BLOCKSIZE="4096"
	BLOCKCOUNT="100"

	# write an object
	io_conduct "WRITE" $src_file $OBJ_ID1 $parity_verify_disabled
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, write failed."
		error_handling $rc
	fi

	echo "Healthy mode write IO completed successfully."
	# read the written object without parity-verify mode.
	io_conduct "READ" $OBJ_ID1  $dest_file $parity_verify_disabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read file differs."
		error_handling $rc
	fi
	echo "Clovis: Healthy mode read IO succeeds."

	# read the object with parity-verify mode.
	io_conduct "READ" $OBJ_ID1  $dest_file $parity_verify_enabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode (parity-verify), read failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode (parity-verify mode), read file differs."
		error_handling $rc
	fi
	echo "Clovis: Healthy mode read IO (parity-verify mode) succeeds."

	echo "Testing IO with a disk failure."

	# Degraded read without parity-verify mode.
	degraded_read 1 $parity_verify_disabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	echo "Degraded read with first disk failure ($fail_device) succeeded"

	# Degraded read with parity-verify mode.
	degraded_read 1 $parity_verify_enabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read (parity-verify mode) failed."
		error_handling $rc
	fi
	echo "Degraded read (parity-verify mode) with first disk failure
	      ($fail_device) succeeded"
	# Test write, when a disk is failed
	io_conduct "WRITE" $src_file $OBJ_ID2 $parity_verify_enabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Object creation and write failed after the failure of
		      the device $fail_device."
		error_handling $rc
	fi
	post_failure_pver_check 1
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded write failed."
		error_handling $rc
	fi

	clovis_inst_cnt=`expr $cnt - 1`
	for i in `seq 1 $clovis_inst_cnt`
	do
		echo "clovis pids=${clovis_pids[$i]}" >> $CLOVIS_TEST_LOGFILE
	done

	echo "Testing IO with a second disk failure."

	# Degraded read without parity-verify mode.
	degraded_read 2 $parity_verify_disabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi

	echo "Degraded read with second disk failure ($fail_device) succeeded"

	# Degraded read with parity verify mode.
	degraded_read 2 $parity_verify_enabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded read failed."
		error_handling $rc
	fi
	echo "Degraded read (parity-verify) with second disk failure
	      ($fail_device) succeeded"

	io_conduct "WRITE" $src_file $OBJ_ID3 $parity_verify_enabled
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Write post second disk failure failed"
		error_handling $rc
	fi

	post_failure_pver_check 2
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Degraded write failed after two failures."
		error_handling $rc
	fi

	unmount_and_clean &>> $MERO_TEST_LOGFILE
	mero_service_stop || rc=1

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}

echo "Synchronous replication IO Test ... "
trap unprepare EXIT
main
report_and_exit degraded-mode-IO $?
