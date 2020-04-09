#!/bin/bash

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh

. $M0_SRC_DIR/utils/functions  # opcode

# If DEBUG_MODE is set to 1, trace file is generated. This may be useful when
# some issue is to be debugged in developer environment.
# Note: Always keep its value 0 while pushing the code to the repository.
DEBUG_MODE=0

mountopt="oostore,verify"
bs=1024
count=1000
st_dir=$M0_SRC_DIR/m0t1fs/linux_kernel/st
revoke_sandbox="$MERO_M0T1FS_TEST_DIR/revoke_sandbox"
source_file="$revoke_sandbox/revoke_source"

revoke_mero_service_start()
{
	local multiple_pools=0
	local rc

	mero_service start $multiple_pools
	rc=$?
	if [ $rc -eq 0 ]
	then
		echo "mero service started"
	else
		echo "Failed to start Mero Service"
	fi
	return $rc
}

revoke_pre()
{
	prog_file_pattern="$st_dir/m0t1fs_io_file_pattern"
	local source_abcd="$revoke_sandbox/revoke_abcd"

	load_mero_ctl_module || return 1

	rm -rf $revoke_sandbox
	mkdir -p $revoke_sandbox
	echo "Creating data file $source_abcd"
	$prog_file_pattern $source_abcd 2>&1 >> $MERO_TEST_LOGFILE || {
		echo "Failed: $prog_file_pattern"
		return 1
	}

	echo "Creating source file $source_file"
	dd if=$source_abcd of=$source_file bs=$bs count=$count >> $MERO_TEST_LOGFILE 2>&1

	echo "ls -l $source_file (Reference for data files generated)"
	ls -l $source_file

	if [ $DEBUG_MODE -eq 1 ]
	then
		rm -f /var/log/mero/m0mero_ko.img
		rm -f /var/log/mero/m0trace.bin*
		$M0_SRC_DIR/utils/trace/m0traced -K -d
	fi
}

revoke_post()
{
	if [ $DEBUG_MODE -eq 1 ]
	then
		pkill -9 m0traced
		# Note: trace may be read as follows
		#$M0_SRC_DIR/utils/trace/m0trace -w0 -s m0t1fs,rpc -I /var/log/mero/m0mero_ko.img -i /var/log/mero/m0trace.bin -o $MERO_TEST_LOGFILE.trace
	fi

	unload_mero_ctl_module || return 1
}

revoke_read_lock()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local s_endpoint="$lnet_nid:12345:33:100"
	local c_endpoint="$lnet_nid:12345:30:*"
	local delay=${1:-5}

	echo "getting write lock..."
	$M0_SRC_DIR/utils/m0rwlock -s $s_endpoint -c $c_endpoint -d $delay &
}

revoke_during_io_test()
{
	local rt_read_file_base="$MERO_M0T1FS_MOUNT_DIR/0:3333"
	local rt_read_nr_files=1
	local rt_cmp_rc
	local rt_rm_rc

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	#echo "Test: Have $rt_read_nr_files data files created on m0t1fs, to be used for read test"
	#for ((i=0; i<$rt_read_nr_files; ++i)); do
	#	echo "creating $rt_read_file_base$i"
	#	touch $rt_read_file_base$i
	#	dd if=$source_file of=$rt_read_file_base$i bs=$bs count=$count &
	#done
	revoke_read_lock 10 || {
		unmount_and_clean
		return 1
	}
	wait

	# Verify that some dd operations were indeed canceled. It is
	# to verify that RPC session was indeed canceled.
	# Many of those read ops fail with the error "Input/output error"
	# while a few fail with the error "Operation canceled"
	#cp $MERO_TEST_LOGFILE "copyoflogfile.txt"
	#num=`grep -n "dd: " $MERO_TEST_LOGFILE | grep "reading" | egrep "("Input\/output\ error")" | grep "$rt_file_base" | wc -l | cut -f1 -d' '`
	#echo "dd read processes canceled : $num"
	#if [ $num -eq 0 ]; then
	#	echo "Failed: No dd reading operation was canceled"
	#	unmount_and_clean
	#	return 1
	#fi

	#num=`grep -n "dd: closing" $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$rt_read_file_base" | wc -l | cut -f1 -d' '`
	#echo "dd closing processes canceled : $num"

	unmount_and_clean
}

revoke_test()
{
    revoke_pre || return 1
    revoke_during_io_test || {
	revoke_post
	return 1
    }
    revoke_post || return 1
}

main()
{
	NODE_UUID=`uuidgen`
	local rc
	local EXECUTOR


	case $1 in
	    test|no-mount)
		;;
	    *)
		echo "Usage: $0 {test|no-mount}"
		return 2
	esac

	echo "*********************************************************"
	echo "Start: rconfc read lock revoke testing"
	echo "*********************************************************"

	sandbox_init

	revoke_mero_service_start || return 1

	case $1 in
	    test)
		echo "Testing revocation during file IO..."
		EXECUTOR=revoke_test
		;;
	    no-mount)
		echo "Revoking read lock with no mounting m0t1fs..."
		EXECUTOR=revoke_read_lock
	esac

	$EXECUTOR  2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=${PIPESTATUS[0]}
	echo "$EXECUTOR rc $rc"
	if [ $rc -ne "0" ]; then
		echo "Failed m0t1fs read lock revocation test."
	fi
	
	sleep 15

	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		if [ $rc -eq "0" ]; then
			rc=1
		fi
	fi

	echo "*********************************************************"
	echo "End: rconfc read lock revoke testing"
	echo "*********************************************************"

	if [ $rc -eq 0 ]; then
		[ $DEBUG_MODE -eq 1 ] || sandbox_fini
	else
		echo "Test log available at $MERO_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main $@
report_and_exit rconfc-read-lock-revoke $?

