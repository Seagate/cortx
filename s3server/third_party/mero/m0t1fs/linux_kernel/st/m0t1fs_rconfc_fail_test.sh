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
DEBUG_MODE=1
FI_OPT="-j"
MD_REDUNDANCY=2

mountopt="oostore,verify"
bs=8192
count=150
st_dir=$M0_SRC_DIR/m0t1fs/linux_kernel/st
rcancel_sandbox="$MERO_M0T1FS_TEST_DIR/rcancel_sandbox"
source_file="$rcancel_sandbox/rcancel_source"
ios2_from_pver0="^s|1:1"

rconfc_fatal_mero_service_start()
{
	local N=2
	local K=1
	local P=4
	local stride=16
	local multiple_pools=1
	local rc

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

rconfc_fatal_pre()
{
	local prog_file_pattern="$st_dir/m0t1fs_io_file_pattern"
	local source_abcd="$rcancel_sandbox/rcancel_abcd"

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

rconfc_fatal_test()
{
	local nr_files=20
	local file_base="$MERO_M0T1FS_MOUNT_DIR/0:1"
	local bs=8192
	local count=150
	local LNET_NID=`lctl list_nids | head -1`
	local FAKE_HA_EP=$LNET_NID:12345:34:1
	local RM_EP=$LNET_NID:12345:33:100
	local CLIENT_EP=$LNET_NID:12345:34:1001
	local rc=

	rconfc_fatal_pre || return 1
	mount_m0t1fs $MERO_M0T1FS_MOUNT_DIR || return 1
	echo "Test: Creating $nr_files files on m0t1fs"
	for ((i=0; i<$nr_files; ++i)); do
		touch $file_base$i || return 1
	done

	echo "Test: Writing to $wc_nr_files files on m0t1fs"
	for ((i=0; i<$nr_files; ++i)); do
		echo "dd if=/dev/urandom of=$file_base$i bs=$bs count=$count &"
		dd if=/dev/urandom of=$file_base$i bs=$bs count=$count &
	done
	echo "Command to jnject a single-time fault on main m0d side"
	env ${TRACE_console:-} $M0_SRC_DIR/console/m0console \
		-f $(opcode M0_FI_COMMAND_OPCODE) -s $FAKE_HA_EP -c $CLIENT_EP\
		-v -d '("mero_ha_entrypoint_rep_rm_fill", "no_rms_fid", 1, 0, 0)'

	echo "Obtain write lock on the cluster"
	$M0_SRC_DIR/utils/m0rwlock -s $RM_EP -c $CLIENT_EP -d 5

	wait
	dd if=/dev/urandom of=${file_base}0 bs=$bs count=$count && \
		die "An error expected but operation completed successfully"
	umount $MERO_M0T1FS_MOUNT_DIR || return 1
}

main()
{
	NODE_UUID=`uuidgen`
	local rc

	echo "*********************************************************"
	echo "Start: m0t1fs rconfc fatal testing"
	echo "*********************************************************"

	sandbox_init

	rconfc_fatal_mero_service_start || return 1

	rconfc_fatal_test 2>&1 | tee -a $MERO_TEST_LOGFILE
	rc=${PIPESTATUS[0]}
	echo "rconfc_fatal_test rc $rc"
	if [ $rc -ne "0" ]; then
		echo "Failed m0t1fs rconfc fatal test."
	fi

	mero_service stop || {
		echo "Failed to stop Mero Service."
		if [ $rc -eq "0" ]; then
			rc=1
		fi
	}

	echo "*********************************************************"
	echo "End: m0t1fs rconfc fatal testing"
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
report_and_exit m0t1fs-rconfc-fatal $?
