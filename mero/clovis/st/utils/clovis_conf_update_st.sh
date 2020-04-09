#!/bin/bash

clovis_st_util_dir=$(dirname $(readlink -f $0))
m0t1fs_dir="$clovis_st_util_dir/../../../m0t1fs/linux_kernel/st"

. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $clovis_st_util_dir/clovis_local_conf.sh
. $clovis_st_util_dir/clovis_st_inc.sh


CLOVIS_TEST_DIR=$SANDBOX_DIR
CLOVIS_TEST_LOGFILE=$SANDBOX_DIR/clovis_`date +"%Y-%m-%d_%T"`.log
CLOVIS_TRACE_DIR=$SANDBOX_DIR/clovis

clean()
{
        multiple_pools=$1
	local i=0
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		# Removes the stob files created in stob domain since
		# there is no support for m0_stob_delete() and after
		# unmounting the client file system, from next mount,
		# fids are generated from same baseline which results
		# in failure of cob_create fops.
		local ios_index=`expr $i + 1`
		rm -rf $CLOVIS_TEST_DIR/d$ios_index/stobs/o/*
	done

        if [ ! -z $multiple_pools ] && [ $multiple_pools == 1 ]; then
		local ios_index=`expr $i + 1`
		rm -rf $CLOVIS_TEST_DIR/d$ios_index/stobs/o/*
        fi
}

mero_service_start()
{
	local multiple_pools=0
	mero_service start $multiple_pools
	if [ $? -ne 0 ]
	then
		echo "Failed to start Mero Service..."
		return 1
	fi
	echo "mero service started"

	ios_eps=""
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done
	return 0
}

mero_service_stop()
{
	mero_service stop
	if [ $? -ne 0 ]
	then
		echo "Failed to stop Mero Service..."
		return 1
	fi
}

error_handling()
{
	rc=$1
	msg=$2
	clean 0 &>>$CLOVIS_TEST_LOGFILE
	mero_service_stop
	echo $msg
	echo "Test log file available at $CLOVIS_TEST_LOGFILE"
	echo "Clovis trace files are available at: $CLOVIS_TRACE_DIR"
	exit $1
}

io_ops()
{
	object_id=$1
	block_size=$2
	block_count=$3
	src_file=$4
	dest_file=$5

	/usr/bin/expect <<EOF
	set timeout 150
	spawn $clovis_st_util_dir/c0client -l $CLOVIS_LOCAL_EP \
		-H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT -P $CLOVIS_PROC_FID \
		> $SANDBOX_DIR/c0client.log
	expect "c0clovis >>"
	send -- "write $object_id $src_file $block_size $block_count\r"
	expect "c0clovis >>"
	send -- "read $object_id $dest_file $block_size $block_count\r"
	expect "c0clovis >>"
	send -- "delete $object_id\r"
	expect "c0clovis >>"
	send -- "quit\r"
EOF
}

revoke_read_lock()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local confd_ep=$CONFD_EP
	local c_ep="$lnet_nid:12345:34:*"
	local delay=${1:-5}
	echo "getting write lock..."
	$M0_SRC_DIR/utils/m0rwlock -s $confd_ep -c $c_ep -d $delay
}


main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	clovis_dgmode_sandbox="$CLOVIS_TEST_DIR/sandbox"
	src_file="$CLOVIS_TEST_DIR/src_file"
	dest_file1="$CLOVIS_TEST_DIR/dest_file1"
	dest_file2="$CLOVIS_TEST_DIR/dest_file2"
	object_id1=1048580
	object_id2=1048581
	block_size=8192
	block_count=4096

	rm -f $src_file $dest_file

	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
	      2> $CLOVIS_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	mkdir $CLOVIS_TRACE_DIR

	# Test IO while read lock revoked (configuration update going on).
	mero_service_start
	dix_init

	revoke_read_lock 30 &
	pid=$!
	io_ops $object_id1 $block_size $block_count $src_file $dest_file1
	wait $pid

	cmp $src_file $dest_file1 2> $CLOVIS_TEST_LOGFILE || {
		rc=$?
		error_handling $rc "Files are different"
	}

	dix_destroy
	mero_service_stop

	# Test read lock revoke (configuration update) while IO going on.
	mero_service_start
	dix_init
	io_ops $object_id2 $block_size $block_count $src_file $dest_file2  &
	pid=$!
	# Let client start
	sleep 10
	revoke_read_lock 15
	wait $pid
	cmp $src_file $dest_file2 2> $CLOVIS_TEST_LOGFILE || {
		rc=$?
		error_handling $rc "Files are different"
	}

	dix_destroy
	clean &>>$CLOVIS_TEST_LOGFILE
	mero_service_stop

}

echo "Configuration Update Test ... "
main
report_and_exit clovis-conf-update $?
