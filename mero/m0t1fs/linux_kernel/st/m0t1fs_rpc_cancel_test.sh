#!/usr/bin/env bash

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

# If INTERNAL_LOOP is set to 1, the various test cases are run iteratively,
# using an infinite loop. This may be useful for testing for some issues
# which may not be consistently reproducible. As compared to running external
# loop over ST, this internal loop approach saves significant time by avoiding
# mero service restart.
# Note: Always keep its value 0 while pushing the code to the repository.
INTERNAL_LOOP=0

# MD_REDUNDANCY used for this ST
# - RCANCEL_MD_REDUNDANCY_1
#   - multiple_pools value is set to 0 in this case, indicating that multiple
#     pools are not to be used.
#   - build_conf() will leave the value of MD_REDUNDANCY to be default,
#     that is 1, in this case.
# - RCANCEL_MD_REDUNDANCY_2
#   - multiple_pools value is set to 1 in this case, indicating that multiple
#     pools are to be used.
#   - build_conf() will set the value of MD_REDUNDANCY to 2, in this case.
#   - This is the default value used in this ST
RCANCEL_MD_REDUNDANCY_1=1
RCANCEL_MD_REDUNDANCY_2=2 # default
rcancel_md_redundancy=$RCANCEL_MD_REDUNDANCY_2

mountopt="oostore,verify"
bs=8192
count=150
st_dir=$M0_SRC_DIR/m0t1fs/linux_kernel/st
rcancel_sandbox="$MERO_M0T1FS_TEST_DIR/rcancel_sandbox"
source_file="$rcancel_sandbox/rcancel_source"
ios2_from_pver0="^s|1:1"

rcancel_mero_service_start()
{
	local N
	local K
	local P
	local stride
	local multiple_pools
	local rc

	echo "About to start Mero service, rcancel_md_redundancy=$rcancel_md_redundancy"
	if [ $rcancel_md_redundancy -eq 1 ]
	then
		N=8
		K=2
		P=20
		stride=32
		multiple_pools=0
	else
		N=2
		K=1
		P=4
		stride=16
		multiple_pools=1
	fi
	mero_service start $multiple_pools $stride $N $K $P
	rc=$?

	if [ $rc -ne 0 ]
	then
		echo "Failed to start Mero services: rc=$rc."
		return 1
	fi
	echo "Mero services have started successfully."

	return 0
}

rcancel_pre()
{
	prog_file_pattern="$st_dir/m0t1fs_io_file_pattern"
	local source_abcd="$rcancel_sandbox/rcancel_abcd"

	load_mero_ctl_module || return 1

	rm -rf $rcancel_sandbox
	mkdir -p $rcancel_sandbox
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

rcancel_post()
{
	if [ $DEBUG_MODE -eq 1 ]
	then
		pkill -9 m0traced
		# Note: trace may be read as follows
		#$M0_SRC_DIR/utils/trace/m0trace -w0 -s m0t1fs,rpc -I /var/log/mero/m0mero_ko.img -i /var/log/mero/m0trace.bin -o $MERO_TEST_LOGFILE.trace
	fi

	unload_mero_ctl_module || return 1
}

rcancel_change_controller_state()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local s_endpoint="$lnet_nid:12345:33:1"
	local c_endpoint="$lnet_nid:$M0HAM_CLI_EP"
	local dev_fid=$1
	local dev_state=$2

	# Generate HA event
	send_ha_events "$dev_fid" "$dev_state" "$s_endpoint" "$c_endpoint"
}

rcancel_session_cancel_fop()
{
	local io_performed_during_cancel=$1

	echo "Test: Cancel session for ios $ios2_from_pver0"
	rcancel_change_controller_state "$ios2_from_pver0" "failed" || {
		return 1
	}
	echo "Test: Session canceled"

	if [ $io_performed_during_cancel -eq 1 ]; then
		echo "Test: sleep 4m so that the processing on the IOS is done that may need to wait for the net buf timeouts."
		echo "Such case is not applicable for production scenario where the IOS would be unreachable before session is canceled."
		sleep 4m
		echo "sleep done"
	fi
}

rcancel_session_restore_fop()
{
	echo "Test: Restore session for ios $ios2_from_pver0"
	rcancel_change_controller_state "$ios2_from_pver0" "online" || {
		return 1
	}
	echo "Test: Session restored"
}

rcancel_mount_cancel_restore_unmount()
{
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1
	rcancel_session_cancel_fop 0
	rcancel_session_restore_fop
	unmount_and_clean
}

rcancel_mount_cancel_unmount()
{
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1
	rcancel_session_cancel_fop 0
	unmount_and_clean
}

rcancel_test_cleanup()
{
	local cl_file_base=$1
	local cl_nr_ops=$2
	local cl_op=$3

	if [ $cl_op != "dd" ] && [ $cl_op != "touch" ] && [ $cl_op != "setfattr" ] && [ $cl_op != "getfattr" ]
	then
		echo "rcancel_test_cleanup(): unsupported op: $cl_op"
		return
	fi

	echo "rcancel_test_cleanup:start"
	echo "pgrep -af $cl_op.*$cl_file_base"
	pgrep -af "$cl_op.*$cl_file_base"

	# Note that this is only an attempt to kiil the processes spawned by
	# the failed test those may be ongoing. There is still a possibility
	# that those processes may not get killed and may affect the
	# subsequent unmount.
        pkill -9 -f "$cl_op.*$cl_file_base"
	echo "rcancel_test_cleanup:end"
	echo "pgrep -af $cl_op.*$cl_file_base"
	pgrep -af "$cl_op.*$cl_file_base"
}

rcancel_issue_writes_n_cancel()
{
	local wc_file_base=$1
	local wc_nr_files=$2

	echo "Test: Creating $wc_nr_files files on m0t1fs"
	for ((i=0; i<$wc_nr_files; ++i)); do
		touch $wc_file_base$i || break
	done

	echo "Test: Writing to $wc_nr_files files on m0t1fs, for concurrent cancel"
	for ((i=0; i<$wc_nr_files; ++i)); do
		echo "dd if=$source_file of=$wc_file_base$i bs=$bs count=$count &"
		dd if=$source_file of=$wc_file_base$i bs=$bs count=$count &
	done

	dd_count=$(jobs | grep "dd.*wc_file_base" | wc -l)
	echo "dd write processes running : $dd_count"
	if [ $dd_count -ne $wc_nr_files ]
	then
		echo "Failed to issue long running $wc_nr_files dd write requests"
		rcancel_test_cleanup $wc_file_base $wc_nr_files "dd"
		return 1
	fi

	# sleep so that enough number of fops are created
	sleep 3
	rcancel_session_cancel_fop 1

	echo "Test: Wait for dd write to finish (ran concurrently with cancel)"
	wait

	# Ensure that all the dd have either failed or finished
	dd_count=$(pgrep -fc dd.*$wc_file_base)
	echo "dd write processes running : $dd_count"
	if [ $dd_count -ne 0 ]
	then
		echo "Failed to complete $wc_nr_files dd requests"
		rcancel_test_cleanup $wc_file_base $wc_nr_files "dd"
		rcancel_session_restore_fop
		return 1
	fi
}

rcancel_cancel_during_write_test()
{
	local wt_write_file_base="$MERO_M0T1FS_MOUNT_DIR/0:2222"
	local wt_write_new_file_base="$MERO_M0T1FS_MOUNT_DIR/0:2299"
	local wt_write_nr_files=20
	local wt_ls_rc
	local wt_cmp_rc
	local wt_rm_rc
	local wt_write_rc

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	# Cancel a session while running some parallel write operations
	rcancel_issue_writes_n_cancel $wt_write_file_base $wt_write_nr_files || {
		unmount_and_clean
		return 1
	}

	# Test ls with canceled session
	# Intentionally keeping 'ls' prior to verifying write cancelation to help
	# troubleshooting, in case required.
	echo "Test: ls for $wt_write_nr_files files before restore (Succeeds if 'Operation canceled' is not received. Shall succeed for all, if rcancel_md_redundancy > 1)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		ls -l $wt_write_file_base$i
		echo "ls_rc: $?"
	done

	# Verify that some dd operations were indeed canceled.
	# It is to verify that some RPC items were indeed canceled through
	# RPC session cancelation. Some items are going to get canceled
	# while attempting to be posted after session cancelation.
	num=`grep -n "dd: " $MERO_TEST_LOGFILE | grep "writing" | grep "Operation canceled" | grep "$wt_write_file_base" | wc -l | cut -f1 -d' '`
	echo "dd write processes canceled : $num"
	if [ $num -eq 0 ]; then
		echo "Failed: No dd writing operation was canceled"
		unmount_and_clean
		return 1
	fi

	num=`grep -n "dd: closing" $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$wt_write_file_base" | wc -l | cut -f1 -d' '`
	echo "dd closing processes canceled : $num"

	num=`grep -n "ls: cannot access" $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$wt_write_file_base" | wc -l | cut -f1 -d' '`
	echo "ls processes canceled : $num"
	if [ $num -gt 0 ] && [ $rcancel_md_redundancy > 1 ]; then
		echo "Failed: ls was canceled inspite-of having rcancel_md_redundancy ($rcancel_md_redundancy) > 1"
		return 1
	fi

	rcancel_session_restore_fop

	echo "Test: ls for $wt_write_nr_files files after restore (written during cancel)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		ls -l $wt_write_file_base$i
		wt_ls_rc=$?
		echo "ls_rc: $wt_ls_rc"
		if [ $wt_ls_rc -ne 0 ]
		then
			echo "ls $wt_write_file_base$i after restore failed"
			unmount_and_clean
			return 1
		fi
	done

	echo "Test: read $wt_write_nr_files files after restore (written during cancel) (They receive either 'No such file or directory', 'EOF' or 'Input/output error' since some gobs/cobs may not be created)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		cmp $source_file $wt_write_file_base$i
		echo "cmp $wt_write_file_base$i: rc $?"
	done

	echo "Test: rm $wt_write_nr_files files after restore (written during cancel) (Expected to fail for the files for which gob is not created)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		rm -f $wt_write_file_base$i
		wt_rm_rc=$?
		echo "rm -f $wt_write_file_base$i: rc $wt_rm_rc"
		if [ $wt_rm_rc -ne 0 ]
		then
			echo "rm failed, $wt_write_file_base$i"
		fi
	done

	echo "Test: write to new $wt_write_nr_files files after restore"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		dd if=$source_file of=$wt_write_new_file_base$i bs=$bs count=$count
		wt_write_rc=$?
		echo "Write to $wt_write_new_file_base$i: rc $wt_write_rc"
		if [ $wt_write_rc -ne 0 ]
		then
			echo "Write $wt_write_file_base$i after restore failed"
			unmount_and_clean
			return 1
		fi
	done

	echo "Test: ls new $wt_write_nr_files files after restore (written after restore)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		ls -l $wt_write_new_file_base$i
		wt_ls_rc=$?
		echo "ls_rc: $wt_ls_rc"
		if [ $wt_ls_rc -ne 0 ]
		then
			echo "ls $wt_write_new_file_base$i after restore failed"
			unmount_and_clean
			return 1
		fi
	done

	echo "Test: Read $wt_write_nr_files files after restore (written after restore)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		cmp $source_file $wt_write_new_file_base$i
		wt_cmp_rc=$?
		echo "cmp $wt_write_new_file_base$i: rc $wt_cmp_rc"
		if [ $wt_cmp_rc -ne 0 ]
		then
			echo "Verification failed: Files differ, $source_file, $wt_write_new_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	echo "Test: rm $wt_write_nr_files files after restore (written after restore)"
	for ((i=0; i<$wt_write_nr_files; ++i)); do
		rm -f $wt_write_new_file_base$i
		wt_rm_rc=$?
		echo "rm -f $wt_write_new_file_base$i: rc $wt_rm_rc"
		if [ $wt_rm_rc -ne 0 ]
		then
			echo "rm failed, $wt_write_new_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	unmount_and_clean
}

rcancel_issue_reads_n_cancel()
{
	local rc_file_base=$1
	local rc_nr_files=$2

	echo "Test: Reading from $rc_nr_files files on m0t1fs, for concurrent cancel"
	for ((i=0; i<$rc_nr_files; ++i)); do
		echo "dd if=$rc_file_base$i of=$rcancel_sandbox/$i bs=$bs count=$count &"
		dd if=$rc_file_base$i of=$rcancel_sandbox/$i bs=$bs count=$count &
	done

	dd_count=$(jobs | grep "dd.*rc_file_base" | wc -l)
	echo "dd read processes running : $dd_count"
	if [ $dd_count -ne $rc_nr_files ]
	then
		echo "Failed to issue long running $rc_nr_files dd read requests"
		rcancel_test_cleanup $rc_file_base $rc_nr_files "dd"
		return 1
	fi

	rcancel_session_cancel_fop 1
	echo "Test: Wait for dd read to finish (ran concurrently with cancel)"
	wait

	# Ensure that all the dd have either failed or finished
	dd_count=$(pgrep -fc dd.*$rc_file_base)
	echo "dd read processes running : $dd_count"
	if [ $dd_count -ne 0 ]
	then
		echo "Failed to complete $rc_nr_files dd requests"
		rcancel_test_cleanup $rc_file_base $rc_nr_files "dd"
		rcancel_session_restore_fop
		return 1
	fi
}

rcancel_cancel_during_read_test()
{
	local rt_read_file_base="$MERO_M0T1FS_MOUNT_DIR/0:3333"
	local rt_read_nr_files=10
	local rt_cmp_rc
	local rt_rm_rc

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	echo "Test: Have $rt_read_nr_files data files created on m0t1fs, to be used for read test"
	for ((i=0; i<$rt_read_nr_files; ++i)); do
		touch $rt_read_file_base$i
		dd if=$source_file of=$rt_read_file_base$i bs=$bs count=$count &
	done
	wait

	echo "Files prepared for read test:"
	for ((i=0; i<$rt_read_nr_files; ++i)); do
		ls -l $rt_read_file_base$i
	done

	# Cancel a session while running some parallel read operations
	rcancel_issue_reads_n_cancel $rt_read_file_base $rt_read_nr_files || {
		unmount_and_clean
		return 1
	}

	# Verify that some dd operations were indeed canceled. It is
	# to verify that RPC session was indeed canceled.
	# Many of those read ops fail with the error "Input/output error"
	# while a few fail with the error "Operation canceled"
	num=`grep -n "dd: " $MERO_TEST_LOGFILE | grep "reading" | egrep 'Operation canceled|Input\/output error' | grep "$rt_file_base" | wc -l | cut -f1 -d' '`
	echo "dd read processes canceled : $num"
	if [ $num -eq 0 ]; then
		echo "Failed: No dd reading operation was canceled"
		unmount_and_clean
		return 1
	fi

	num=`grep -n "dd: closing" $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$rt_read_file_base" | wc -l | cut -f1 -d' '`
	echo "dd closing processes canceled : $num"

	# Test ls with canceled session
	echo "Test: ls for $rt_read_nr_files files before restore (Succeeds if "Operation canceled" is not received. Shall succeed for all, if rcancel_md_redundancy > 1)"
	for ((i=0; i<$rt_read_nr_files; ++i)); do
		ls -l $rt_read_file_base$i
		echo "ls_rc: $?"
	done
	num=`grep -n "ls: cannot access" $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$rt_read_file_base" | wc -l | cut -f1 -d' '`
	echo "ls processes canceled : $num"
	if [ $num -gt 0 ] && [ $rcancel_md_redundancy > 1 ]; then
		echo "Failed: ls was canceled inspite-of having rcancel_md_redundancy ($rcancel_md_redundancy) > 1"
		return 1
	fi

	rcancel_session_restore_fop

	# Verify contents of files after restore to ensure that there is no
	# corruption
	echo "Test: read $rt_read_nr_files files after restore (written before cancel)"
	for ((i=0; i<$rt_read_nr_files; ++i)); do
		file_from_m0t1fs=$rt_read_file_base$i
		cmp $source_file $file_from_m0t1fs
		rt_cmp_rc=$?
		echo "cmp $file_from_m0t1fs: rc $rt_cmp_rc"
		if [ $rt_cmp_rc -ne 0 ]
		then
			echo "Verification failed: Files differ, $source_file, $file_from_m0t1fs"
			unmount_and_clean
			return 1
		fi
	done

	echo "Test: rm $rt_read_nr_files files after restore (written before cancel)"
	for ((i=0; i<$rt_read_nr_files; ++i)); do
		rm -f $rt_read_file_base$i
		rt_rm_rc=$?
		echo "rm -f $rt_read_file_base$i: rc $rt_rm_rc"
		if [ $rt_rm_rc -ne 0 ]
		then
			echo "rm failed, $rt_read_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	unmount_and_clean
}

rcancel_cancel_during_create_test()
{
	local create_nr_files=20
	local create_file_base="$MERO_M0T1FS_MOUNT_DIR/0:4444"

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	echo "Test: Enable FP to drop create reply items"
	echo 'enable item_reply_received_fi drop_create_item_reply always' > /sys/kernel/debug/mero/finject/ctl

	echo "Test: Create $create_nr_files files on m0t1fs"
	for ((i=0; i<$create_nr_files; ++i)); do
		echo "touch $create_file_base$i &"
		touch $create_file_base$i &
	done

	create_count=$(jobs | grep "touch.*create_file_base" | wc -l)
	echo "create processes running : $create_count"
	if [ $create_count -ne $create_nr_files ]
	then
		echo 'disable item_reply_received_fi drop_create_item_reply' > /sys/kernel/debug/mero/finject/ctl
		echo "Failed to issue long running $create_nr_files create requests"
		rcancel_test_cleanup $create_file_base $create_nr_files "touch"
		unmount_and_clean
		return 1
	fi

	sleep 2
	rcancel_session_cancel_fop 0

	echo 'disable item_reply_received_fi drop_create_item_reply' > /sys/kernel/debug/mero/finject/ctl
	echo "Test: Wait for create ops to finish (ran concurrently with cancel)"
	wait

	# Ensure that all create ops have either failed or finished
	create_count=$(pgrep -fc touch)
	echo "create processes running : $create_count"
	if [ $create_count -ne 0 ]
	then
		echo "Failed to complete $create_nr_files create requests"
		rcancel_test_cleanup $create_file_base $create_nr_files "touch"
		rcancel_session_restore_fop
		unmount_and_clean
		return 1
	fi

	echo "Test: ls $create_nr_files files after cancel"
	for ((i=0; i<$create_nr_files; ++i)); do
		echo "ls -l $create_file_base$i"
		ls -l $ct_create_file_base$i
	done

	rcancel_session_restore_fop

	# Verify that some create operations were indeed canceled due to
	# RPC session cancelation.
	num=`grep -n "touch: " $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$ct_create_file_base" | wc -l | cut -f1 -d' '`
	echo "create processes canceled : $num"
	if [ $num -eq 0 ]; then
		echo "Failed: No create operation was canceled"
		unmount_and_clean
		return 1
	fi

	echo "Test: rm $create_nr_files files after restore"
	for ((i=0; i<$create_nr_files; ++i)); do
		rm -f $create_file_base$i
		rm_rc=$?
		echo "rm -f $create_file_base$i: rc $rm_rc"
		if [ $rm_rc -ne 0 ]
		then
			echo "rm failed, $create_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	unmount_and_clean
}

rcancel_cancel_during_delete_test()
{
	local delete_nr_files=10
	local delete_file_base="$MERO_M0T1FS_MOUNT_DIR/0:5555"

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	echo "Test: Have $delete_nr_files files created on m0t1fs, to be used for delete op"
	i=0
	touch $delete_file_base$i # 0 size file, create half of those as empty
	for ((i=1; i<$delete_nr_files; ++i)); do
		touch $delete_file_base$i
		dd if=$source_file of=$delete_file_base$i bs=$bs count=$count &
	done
	wait

	# Files prepared for delete test
	for ((i=0; i<$delete_nr_files; ++i)); do
		ls -l $delete_file_base$i
	done

	echo "Test: Enable FP to drop delete reply items"
	echo 'enable item_reply_received_fi drop_delete_item_reply always' > /sys/kernel/debug/mero/finject/ctl

	echo "Test: Delete $delete_nr_files files from m0t1fs"
	for ((i=0; i<$delete_nr_files; ++i)); do
		echo "rm -f $delete_file_base$i &"
		rm -f $delete_file_base$i &
	done

	delete_count=$(jobs | grep "rm.*delete_file_base" | wc -l)
	echo "delete processes running : $delete_count"
	if [ $delete_count -ne $delete_nr_files ]
	then
		echo 'disable item_reply_received_fi drop_delete_item_reply' > /sys/kernel/debug/mero/finject/ctl
		echo "Failed to issue long running $delete_nr_files delete requests"
		rcancel_test_cleanup $delete_file_base $delete_nr_files "rm"
		unmount_and_clean
		return 1
	fi

	sleep 2
	rcancel_session_cancel_fop 0

	echo 'disable item_reply_received_fi drop_delete_item_reply' > /sys/kernel/debug/mero/finject/ctl
	echo "Test: Wait for delete ops to finish (ran concurrently with cancel)"
	wait

	# Ensure that all the file ops have either failed or finished
	delete_count=$(pgrep -fc rm.*$delete_file_base)
	echo "delete processes running : $delete_count"
	if [ $delete_count -ne 0 ]
	then
		echo "Failed to complete $delete_nr_files delete requests"
		rcancel_test_cleanup $delete_file_base $delete_nr_files "rm"
		rcancel_session_restore_fop
		unmount_and_clean
		return 1
	fi

	echo "Test: ls $delete_nr_files files after cancel"
	for ((i=0; i<$delete_nr_files; ++i)); do
		echo "ls -l $delete_file_base$i"
		ls -l $delete_file_base$i
	done

	rcancel_session_restore_fop

	# Verify that some delete operations were indeed canceled due to
	# RPC session cancelation.
	num=`grep -n "rm: " $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$delete_file_base" | wc -l | cut -f1 -d' '`
	echo "delete processes canceled : $num"
	if [ $num -eq 0 ]; then
		echo "Failed: No delete operation was canceled"
		unmount_and_clean
		return 1
	fi

	echo "Test: rm $delete_nr_files files after restore"
	for ((i=0; i<$delete_nr_files; ++i)); do
		rm -f $delete_file_base$i
		rm_rc=$?
		echo "rm -f $delete_file_base$i: rc $rm_rc"
		if [ $rm_rc -ne 0 ]
		then
			echo "rm failed, $delete_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	unmount_and_clean
}

rcancel_cancel_during_setfattr_ops_test()
{
	local setfattr_nr_files=20
	local setfattr_file_base="$MERO_M0T1FS_MOUNT_DIR/0:6666"

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	echo "Test: Have $setfattr_nr_files files created on m0t1fs, to be used for setfattr op"
	for ((i=0; i<$setfattr_nr_files; ++i)); do
		echo "touch $setfattr_file_base$i"
		touch $setfattr_file_base$i
	done

	echo "Test: Enable FP to drop setattr reply items"
	echo 'enable item_received_fi drop_setattr_item_reply always' > /sys/kernel/debug/mero/finject/ctl

	echo "Test: setfattr for $setfattr_nr_files files on m0t1fs"
	for ((i=0; i<$setfattr_nr_files; ++i)); do
		echo "setfattr -n lid -v 4 $setfattr_file_base$i &"
		setfattr -n lid -v 4 $setfattr_file_base$i &
	done

	setfattr_count=$(jobs | grep "setfattr.*setfattr_file_base" | wc -l)
	echo "setfattr processes running : $setfattr_count"
	if [ $setfattr_count -ne $setfattr_nr_files ]
	then
		echo 'disable item_received_fi drop_setattr_item_reply' > /sys/kernel/debug/mero/finject/ctl
		echo "Failed to issue long running $setfattr_nr_files setfattr requests"
		rcancel_test_cleanup $setfattr_file_base $setfattr_nr_files "setfattr"
		unmount_and_clean
		return 1
	fi

	sleep 2
	rcancel_session_cancel_fop 0

	echo "Test: Disable FP to drop setattr reply items"
	echo 'disable item_received_fi drop_setattr_item_reply' > /sys/kernel/debug/mero/finject/ctl
	echo "Test: Wait for setfattr ops to finish (ran concurrently with cancel)"
	wait

	# Ensure that all the file ops have either failed or finished
	setfattr_count=$(pgrep -fc setfattr)
	echo "setfattr processes running : $(($setfattr_count))"
	if [ $setfattr_count -ne 0 ]
	then
		echo "Failed to complete $setfattr_nr_files setfattr requests"
		rcancel_test_cleanup $setfattr_file_base $setfattr_nr_files "setfattr"
		rcancel_session_restore_fop
		unmount_and_clean
		return 1
	fi

	sleep 2
	rcancel_session_restore_fop

	# Verify that some setfattr operations were indeed canceled due to
	# RPC session cancelation.
	num=`grep -n "setfattr: " $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$setfattr_file_base" | wc -l | cut -f1 -d' '`
	echo "setfattr processes canceled : $num"
	if [ $num -eq 0 ]; then
		echo "Failed: No setfattr operation was canceled"
		unmount_and_clean
		return 1
	fi

	echo "Test: rm $setfattr_nr_files files after restore"
	for ((i=0; i<$setfattr_nr_files; ++i)); do
		rm -f $setfattr_file_base$i
		rm_rc=$?
		echo "rm -f $setfattr_file_base$i: rc $rm_rc"
		if [ $rm_rc -ne 0 ]
		then
			echo "rm failed, $setfattr_file_base$i"
			unmount_and_clean
			return 1
		fi
	done

	unmount_and_clean
}

rcancel_cancel_during_getfattr_ops_test()
{
	local getfattr_nr_files=20
	local getfattr_file_base="$MERO_M0T1FS_MOUNT_DIR/0:7777"

	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR $mountopt || return 1

	echo "Test: Have $getfattr_nr_files files created on m0t1fs, to be used for getfattr op"
	for ((i=0; i<$getfattr_nr_files; ++i)); do
		echo "touch $getfattr_file_base$i"
		touch $getfattr_file_base$i
	done

	echo "Test: Enable FP to drop getattr reply items"
	echo 'enable item_received_fi drop_getattr_item_reply always' > /sys/kernel/debug/mero/finject/ctl

	echo "Test: getfattr for $getfattr_nr_files files on m0t1fs"
	for ((i=0; i<$getfattr_nr_files; ++i)); do
		echo "getfattr -n lid $getfattr_file_base$i &"
		getfattr -n lid $getfattr_file_base$i &
	done

	getfattr_count=$(jobs | grep "getfattr.*getfattr_file_base" | wc -l)
	echo "getfattr processes running : $(($getfattr_count))"
	if [ $getfattr_count -ne $getfattr_nr_files ]
	then
		echo 'disable item_received_fi drop_getattr_item_reply' > /sys/kernel/debug/mero/finject/ctl
		echo "Failed to issue long running $getfattr_count getfattr requests"
		rcancel_test_cleanup $getfattr_file_base $getfattr_nr_files "getfattr"
		unmount_and_clean
		return 1
	fi

	rcancel_session_cancel_fop 0

	echo "Test: Disable FP to drop getattr reply items"
	echo 'disable item_received_fi drop_getattr_item_reply' > /sys/kernel/debug/mero/finject/ctl
	echo "Test: Wait for getfattr ops to finish (ran concurrently with cancel)"
	wait

	# Ensure that all the file ops have either failed or finished
	getfattr_count=$(pgrep -fc getfattr)
	echo "getfattr processes running : $(($getfattr_count))"
	if [ $getfattr_count -ne 0 ]
	then
		echo "Failed to complete $getfattr_nr_files getfattr ops"
		rcancel_test_cleanup $getfattr_file_base $getfattr_nr_files "getfattr"
		rcancel_session_restore_fop
		unmount_and_clean
		return 1
	fi

	rcancel_session_restore_fop

	# Verify that some getfattr operations were indeed canceled due to
	# RPC session cancelation.
	num=`grep -n "getfattr: " $MERO_TEST_LOGFILE | grep "Operation canceled" | grep "$getfattr_file_base" | wc -l | cut -f1 -d' '`
	echo "getfattr processes canceled : $num"
	if [ $num -eq 0 ]; then
		if [ $rcancel_md_redundancy -eq $RCANCEL_MD_REDUNDANCY_1 ]; then
			echo "Failed: No getfattr operation was canceled"
			unmount_and_clean
			return 1
		else
			echo "No getfattr op was canceled, as expected, since rcancel_md_redundancy=$rcancel_md_redundancy"
		fi
	fi

	echo "Test: rm $getfattr_nr_files files after restore"
	for ((i=0; i<$getfattr_nr_files; ++i)); do
		rm -f $getfattr_file_base$i
		rm_rc=$?
		echo "rm -f $getfattr_file_base$i: rc $rm_rc"
		if [ $rm_rc -ne 0 ]
		then
			echo "rm failed, $getfattr_file_base$i"
			unmount_and_clean
			return 1
		fi
	done
	unmount_and_clean
}

rcancel_test_cases()
{
	echo "======================================================="
	echo "TC.1.Start: mount-cancel-restore-unmount"
	echo "======================================================="
	rcancel_mount_cancel_restore_unmount || return 1
	echo "======================================================="
	echo "TC.1.End: mount-cancel-restore-unmount"
	echo "======================================================="

	echo "======================================================="
	echo "TC.2.Start: mount-cancel-unmount"
	echo "======================================================="
	rcancel_mount_cancel_unmount || return 1
	echo "======================================================="
	echo "TC.2.End: mount-cancel-unmount"
	echo "======================================================="

	echo "======================================================="
	echo "TC.3.Start: Session cancel with concurrent write ops"
	echo "======================================================="
	rcancel_cancel_during_write_test || return 1
	echo "======================================================="
	echo "TC.3.End: Session cancel with concurrent write ops"
	echo "======================================================="

	echo "======================================================="
	echo "TC.4.Start: Session cancel with concurrent read ops"
	echo "======================================================="
	rcancel_cancel_during_read_test || return 1
	echo "======================================================="
	echo "TC.4.End: Session cancel with concurrent read ops"
	echo "======================================================="

	echo "======================================================="
	echo "TC.5.Start: Session cancel with concurrent create ops"
	echo "======================================================="
	rcancel_cancel_during_create_test || return 1
	echo "======================================================="
	echo "TC.5.End: Session cancel with concurrent create ops"
	echo "======================================================="

	echo "======================================================="
	echo "TC.6.Start: Session cancel with concurrent delete ops"
	echo "======================================================="
	rcancel_cancel_during_delete_test || return 1
	echo "======================================================="
	echo "TC.6.End: Session cancel with concurrent delete ops"
	echo "======================================================="

	echo "======================================================="
	echo "TC.7.Start: Session cancel with concurrent setfattr ops"
	echo "======================================================="
	rcancel_cancel_during_setfattr_ops_test || return 1
	echo "======================================================="
	echo "TC.7.End: Session cancel with concurrent setfattr ops"
	echo "======================================================="

	echo "======================================================="
	echo "TC.8.Start: Session cancel with concurrent getfattr ops"
	echo "======================================================="
	rcancel_cancel_during_getfattr_ops_test || return 1
	echo "======================================================="
	echo "TC.8.End: Session cancel with concurrent getfattr ops"
	echo "======================================================="

	num=`grep -n "Connection timed out" $MERO_TEST_LOGFILE | wc -l | cut -f1 -d' '`
	echo "Connection timed out : $num"
	if [ $num -gt 0 ]; then
		echo "Failed: Connection timed out error has occurred"
		return 1
	fi
}

rcancel_test()
{
	local while_loop_i=0

	rcancel_pre || return 1

	if [ $INTERNAL_LOOP -eq 1 ]
	then
		while true; do
			echo "while_loop_i $while_loop_i"
			echo "==========================="
			while_loop_i=`expr $while_loop_i \+ 1`

			# Wipe out contents from MERO_TEST_LOGFILE
			echo "" > $MERO_TEST_LOGFILE

			rcancel_test_cases || {
				rcancel_post
				return 1
			}
		done
	else
		rcancel_test_cases || {
			rcancel_post
			return 1
		}
	fi
	rcancel_post || return 1
}

main()
{
	NODE_UUID=`uuidgen`
	local rc

	echo "*********************************************************"
	echo "Start: RPC session cancellation testing"
	echo "*********************************************************"

	sandbox_init

	rcancel_mero_service_start || return 1

	rcancel_test 2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=${PIPESTATUS[0]}
	echo "rcancel_test rc $rc"
	if [ $rc -ne "0" ]; then
		echo "Failed m0t1fs RPC Session Cancel test."
	fi

	mero_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Mero Service."
		if [ $rc -eq "0" ]; then
			rc=1
		fi
	fi

	echo "*********************************************************"
	echo "End: RPC session cancellation testing"
	echo "*********************************************************"

	if [ $rc -eq 0 ]; then
		[ $DEBUG_MODE -eq 1 ] || sandbox_fini
	else
		echo "Test log available at $MERO_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit rpc-session-cancel $?
