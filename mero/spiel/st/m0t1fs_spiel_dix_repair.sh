#!/usr/bin/env bash
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/spiel/st/m0t1fs_spiel_dix_common_inc.sh


spiel_dix_repair_test()
{
	local fail_device1=2

	echo "Starting DIX repair testing ..."

#	echo "Destroy Meta"
#	$DIXINIT_TOOL_DESTROY >/dev/null
#	echo "Create Meta"
#	$DIXINIT_TOOL_CREATE >/dev/null
	echo "*** m0dixinit is omitted. Mkfs creates meta indices now."

	verify || return $?
	#######################################################################
	#  Now starting SPIEL DIX repair/rebalance abort/continue testing     #
	#######################################################################

	echo "Set Failure device: $fail_device1"
	cas_disk_state_set "failed" $fail_device1 || return $?

	echo "Device $fail_device1 failed. Do dgmode read"
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	echo "Start DIX repair (1)."
	cas_disk_state_set "repair" $fail_device1 || return $?
	spiel_dix_repair_start
	sleep 2

	echo "Abort DIX repair (1)."
	spiel_dix_repair_abort

	echo "Wait for DIX repair (1)."
	spiel_wait_for_dix_repair || return $?
	verify || return $?

	echo "Start DIX repair again (2).."
	spiel_dix_repair_start
	sleep 3

	cas_disk_state_set "repaired" $fail_device1 || return $?
	echo "DIX Repair done."
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	cas_disk_state_set "rebalance" $fail_device1 || return $?
	echo "Start DIX Re-balance (1)."
	spiel_dix_rebalance_start
	sleep 2

	echo "Abort DIX Re-balance (1)."
	spiel_dix_rebalance_abort

	echo "Wait for DIX Re-balance (1)."
	spiel_wait_for_dix_rebalance || return $?
	verify || return $?

	echo "Start DIX Re-balance again (2)."
	spiel_dix_rebalance_start
	sleep 2

	echo "Wait for DIX Re-balance (2)."
	spiel_wait_for_dix_rebalance || return $?
	verify || return $?

	cas_disk_state_set "online" $fail_device1 || return $?
	echo "DIX Rebalance done."

	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	#######################################################################
	#  End                                                                #
	#######################################################################

	return 0
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

	spiel_prepare

	if [[ $rc -eq 0 ]] && ! spiel_dix_repair_test ; then
		echo "Failed: DIX repair failed.."
		rc=1
	fi

	spiel_cleanup

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
report_and_exit spiel-dix-repair $?
