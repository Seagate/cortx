#!/usr/bin/env bash
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/spiel/st/m0t1fs_spiel_sns_common_inc.sh


spiel_sns_repair_and_rebalance_test()
{
	local fail_device1=1
	local fail_device2=9
	local fail_device3=3

	local_write $src_bs $src_count || return $?

	echo "Starting SNS repair testing ..."
	for ((i=0; i < ${#files[*]}; i++)) ; do
		touch_file $MERO_M0T1FS_MOUNT_DIR/${files[$i]} ${unit_size[$i]}
		_dd ${files[$i]} $((${unit_size[$i]} * 1024)) ${file_size[$i]}
	done

	verify || return $?

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	mount | grep m0t1fs

	#######################################################################
	#  Now starting SPIEL sns repair/rebalance abort/continue testing     #
	#######################################################################

	echo "Set Failure device: $fail_device1 $fail_device2"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "Device $fail_device1 and $fail_device2 failed. Do dgmode read"
	verify || return $?

	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Start SNS repair (1)."
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	spiel_sns_repair_start
	sleep 2

	echo "Abort SNS repair (1)."
	spiel_sns_repair_abort

	echo "wait for sns repair (1)."
	spiel_wait_for_sns_repair || return $?

	verify || return $?

	echo "start SNS repair again (2).."
	spiel_sns_repair_start
	sleep 3

	# Another device failed during the above SNS repair.
	# We need to abort current SNS first, and then we start SNS repair again.
	echo "Abort SNS repair (2).."
	spiel_sns_repair_abort

	echo "wait for sns repair abort (2).."
	spiel_wait_for_sns_repair || return $?

        echo "failing another device ($fail_device3)"
        disk_state_set "failed" $fail_device3 || return $?
	disk_state_set "repair" $fail_device3 || return $?

	echo "start SNS repair again (3)..."
	spiel_sns_repair_start
	sleep 3

	echo "wait for the third sns repair (3)..."
	spiel_wait_for_sns_repair || return $?

	disk_state_set "repaired" $fail_device1 $fail_device2 $fail_device3 || return $?
	echo "SNS Repair done."
	verify || return $?

	disk_state_get $fail_device1 $fail_device2 $fail_device3 || return $?

	disk_state_set "rebalance" $fail_device1 $fail_device2 $fail_device3 || return $?
	disk_state_get $fail_device1 $fail_device2 $fail_device3 || return $?
        sleep 2
	echo "Starting SNS Re-balance.. (1)"
	spiel_sns_rebalance_start
	sleep 2
	echo "wait for sns rebalance"
	spiel_wait_for_sns_rebalance || return $?
	disk_state_set "online" $fail_device1 $fail_device2 $fail_device3 || return $?
	echo "SNS Rebalance done."

	verify || return $?

	disk_state_get $fail_device1 $fail_device2 $fail_device3 || return $?

        echo "Testing SNS rebalance abort with new disk failure..."
	rebalance_abort 1 9

	echo "Testing SNS rebalance abort with repaired disk failure..."
	rebalance_abort 1 1
	#######################################################################
	#  End                                                                #
	#######################################################################

	return 0
}

test_repaired_device_failure()
{
	local fail_device1=$1

	disk_state_get $fail_device1 || return $?

	disk_state_set "rebalance" $fail_device1 || return $?
	echo "Starting SNS Rebalance.."
	spiel_sns_rebalance_start

        echo "wait for sns Re-balance"
        spiel_wait_for_sns_rebalance || return $?
	sleep 2

	disk_state_set "online" $fail_device1 || return $?
	echo "SNS Rebalance done."

	verify || return $?
	disk_state_get $fail_device1 || return $?
}

test_new_device_failure()
{
	local fail_device1=$1
	local fail_device2=$2

	echo "Set $fail_device2 to "failed""
	disk_state_set "failed" $fail_device2 || return $?
	disk_state_set "repair" $fail_device2 || return $?

	echo "Start SNS repair again"
	spiel_sns_repair_start
	sleep 2

	echo "wait for the sns repair"
	spiel_wait_for_sns_repair || return $?
	verify || return $?

	disk_state_set "repaired" $fail_device2 || return $?

	disk_state_get $fail_device1 $fail_device2 || return $?

	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	echo "Starting SNS Rebalance.."
	spiel_sns_rebalance_start

        echo "wait for sns Re-balance"
        spiel_wait_for_sns_rebalance || return $?
	sleep 2

	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Rebalance done."

	verify || return $?
	disk_state_get $fail_device1 $fail_device2 || return $?
}

rebalance_abort()
{
	local fail_device1=$1
	local fail_device2=$2

	echo "Set Failure device: $fail_device1"
	disk_state_set "failed" $fail_device1 || return $?

	echo "Start SNS repair."
        echo "set $fail_device1 to repairing"
	disk_state_set "repair" $fail_device1 || return $?
	spiel_sns_repair_start
	sleep 2

	echo "wait for sns repair to finish."
	spiel_wait_for_sns_repair || return $?

	disk_state_set "repaired" $fail_device1 || return $?
	echo "SNS Repair done."
	verify || return $?

	disk_state_set "rebalance" $fail_device1 || return $?
	disk_state_get $fail_device1 || return $?
        sleep 2
	echo "Starting SNS Re-balance.."
	spiel_sns_rebalance_start
	sleep 2
        echo "Abort SNS Rebalance"
        spiel_sns_rebalance_abort
        echo "wait for sns Re-balance abort (1)"
        spiel_wait_for_sns_rebalance || return $?

	echo "Set $fail_device1 back to "repaired""
	disk_state_set "repaired" $fail_device1 || return $?
	if [ $fail_device1 -eq $fail_device2 ]
	then
		test_repaired_device_failure $fail_device1
	else
		test_new_device_failure $fail_device1 $fail_device2
	fi
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	mero_service start $multiple_pools $stride $N $K $P || {
		echo "Failed to start Mero Service."
		return 1
	}

	sns_repair_mount || rc=$?

	spiel_prepare

	if [[ $rc -eq 0 ]] && ! spiel_sns_repair_and_rebalance_test ; then
		echo "Failed: SNS repair failed.."
		rc=1
	fi

	spiel_cleanup

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
report_and_exit spiel-sns-repair $?
