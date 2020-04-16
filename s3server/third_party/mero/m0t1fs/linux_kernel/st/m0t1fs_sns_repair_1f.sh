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
file=(
	0:10000
	0:10001
	0:10002
)

file_size=(
	50
	70
	30
)

N=2
K=1
P=4
stride=32
src_bs=10M
src_count=2

sns_repair_test()
{
	local rc=0
	local fail_device=1
	local unit_size=$((stride * 1024))

	echo "Starting SNS repair testing ..."

	local_write $src_bs $src_count || return $?

	for ((i=0; i < ${#file[*]}; i++)) ; do
		_dd ${file[$i]} $unit_size ${file_size[$i]} || return $?
		_md5sum ${file[$i]} || return $?
	done

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

####### Set Failure device
	disk_state_set "failed" $fail_device || return $?

	disk_state_get $fail_device || return $?

	echo "Device $fail_device failed. Do dgmode read"
	md5sum_check || return $?

	disk_state_set "repair" $fail_device || return $?
	sns_repair || return $?

	echo "**** Test spurious rebalance request during repair, this should fail *****"
	disk_state_set "rebalance" $fail_device || return $?
	disk_state_get $fail_device || return $?

	sns_rebalance

	# Wait for rebalance request to complete.
	wait_for_sns_repair_or_rebalance "rebalance"
	disk_state_set "online" $fail_device

	disk_state_get $fail_device
	echo "**** Spurious rebalance event test completed *****"

	# Make sure that repair is complete.
	wait_for_sns_repair_or_rebalance "repair"

	disk_state_set "failed" $fail_device

	disk_state_get $fail_device

	echo "Device $fail_device failed. Do dgmode read"
	md5sum_check || return $?

	disk_state_set "repair" $fail_device || return $?
	sns_repair || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	disk_state_set "repaired" $fail_device || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	echo "SNS Repair done."
	md5sum_check || return $?

	echo "Query device state"
	disk_state_get $fail_device || return $?

	disk_state_set "rebalance" $fail_device || return $?
	echo "Starting SNS Re-balance.."
	sns_rebalance || return $?

	echo "**** Test spurious repair event during rebalance, this should fail  *****"
	disk_state_set "failed" $fail_device || return $?
	disk_state_get $fail_device || return $?

	disk_state_set "repair" $fail_device || return $?
	sns_repair

	# Wait for repair to complete.
	wait_for_sns_repair_or_rebalance "repair"
	disk_state_set "repaired" $fail_device

	disk_state_get $fail_device
	echo "**** Spurious repair event test completed *****"

	# Make sure rebalance is complete.
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	disk_state_set "rebalance" $fail_device
	echo "Starting SNS Re-balance.."
	sns_rebalance || return $?

	echo "wait for sns rebalance"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	disk_state_set "online" $fail_device || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "rebalance" || return $?

	disk_state_get $fail_device

	echo "SNS Re-balance done."

	echo "Verifying checksums.."
	md5sum_check || return $?

	return $?
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

	if [[ $rc -eq 0 ]] && ! sns_repair_test ; then
		echo "Failed: SNS repair failed.."
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
report_and_exit sns-single $?
