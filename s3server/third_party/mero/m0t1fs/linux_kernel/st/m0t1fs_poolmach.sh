#!/usr/bin/env bash

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh

pool_mach_test()
{
	local ios_eps
	rc=0

	echo "Testing pool machine.."
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

####### Query
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 2
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Query again
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 1
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 2
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 3
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Query again
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo $trigger
	eval $trigger
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

	return $rc
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	mero_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
		return 1
	fi

	rc=0
	pool_mach_test || {
		echo "Failed: pool machine failure."
		rc=1
	}

	mero_service stop
	if [ $? -ne 0 ]; then
		echo "Failed to stop Mero Service."
		return 1
	fi

	sandbox_fini $rc
	return $rc
}

trap unprepare EXIT
main
report_and_exit poolmach $?
