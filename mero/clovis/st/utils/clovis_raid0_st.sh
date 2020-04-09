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
K=0
P=15
stride=32
BLOCKSIZE=""
BLOCKCOUNT=""
OBJ_ID1="1048577"
OBJ_ID2="1048578"
# The second half is hex representations of OBJ_ID1 and OBJ_ID2.
OBJ_HID1="0:100001"
OBJ_HID2="0:100002"
PVER_1="7600000000000001:a"
PVER_2="7680000000000001:42"
read_verify="true"
clovis_pids=""
export cnt=1

CLOVIS_TEST_DIR=$SANDBOX_DIR
CLOVIS_TEST_LOGFILE=$SANDBOX_DIR/clovis_`date +"%Y-%m-%d_%T"`.log
CLOVIS_TRACE_DIR=$SANDBOX_DIR/clovis

raid0_test()
{
	local singlenode=$1

	if [[ $singlenode == "true" ]]; then
	# A global variable present in m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
        # indicating whether to launch a single IOS or multiple.
		SINGLE_NODE=1
		P=1
	fi

	mero_service_start $N $K $P $stride

	#Initialise dix
	dix_init

	# write an object
	io_conduct "WRITE" $src_file $OBJ_ID1 "false"
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, write failed."
		error_handling $rc
	fi
	echo "Healthy mode write succeeds."

	# read the written object
	io_conduct "READ" $OBJ_ID1  $dest_file "false"
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
	echo "Healthy mode, read file succeeds."
	# Read verification is meaningless
        # in case of RAID0 type layout, but the code should get
        # executed seamlessly.
	io_conduct "READ" $OBJ_ID1  $dest_file $read_verify
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read verify failed."
		error_handling $rc
	fi
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Healthy mode, read file differs."
		error_handling $rc
	fi
	echo "Healthy mode, read file with parity verify succeeds"
	echo "Clovis: Healthy mode IO succeeds."

	mero_service_stop || rc=1
	return $rc
}

main()
{

	sandbox_init

	NODE_UUID=`uuidgen`
	src_file="$CLOVIS_TEST_DIR/clovis_source"
	dest_file="$CLOVIS_TEST_DIR/clovis_dest"
	rc=0

	BLOCKSIZE=4096
	BLOCKCOUNT=25
	echo "dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of=$src_file"
	dd if=/dev/urandom bs=$BLOCKSIZE count=$BLOCKCOUNT of=$src_file \
              2> $CLOVIS_TEST_LOGFILE || {
		echo "Failed to create a source file"
		return 1
	}

	mkdir $CLOVIS_TRACE_DIR

	raid0_test false
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Clovis IO test with N=$N failed for P=$P"
		return $rc
	fi
	raid0_test true
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Clovis IO test with singlenode configuration failed"
		return $rc
	fi

	sandbox_fini
	return 0
}

echo "CLOVIS SINGLENODE IO Test"
trap unprepare EXIT
main
report_and_exit clovis-raid0-io $?
