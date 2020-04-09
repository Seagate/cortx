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
ios2_from_pver0="^s|1:1"

N=3
K=2
P=15
stride=4

clovis_change_controller_state()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local s_endpoint="$lnet_nid:12345:34:101"
	local c_endpoint="$lnet_nid:$M0HAM_CLI_EP"
	local dev_fid=$1
	local dev_state=$2

	# Generate HA event
	echo "Send HA event for clovis"
	echo "c_endpoint is : $c_endpoint"
	echo "s_endpoint is : $s_endpoint"

	send_ha_events "$dev_fid" "$dev_state" "$s_endpoint" "$c_endpoint"
}

clovis_online_session_fop()
{
	echo "Send online fop ha event"
	clovis_change_controller_state "$ios2_from_pver0" "online"
	return 0
}

clovis_cancel_session_fop()
{
	echo "Send cancel fop ha event"
	clovis_change_controller_state "$ios2_from_pver0" "failed"
	return 0
}

clovis_cancel_during_write()
{
	object_id=$1;
	block_size=$2;
	block_count=$3;
	src_file=$4;
	dest_file=$5;
	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT  \
	-P $CLOVIS_PROC_FID"
	CLOVIS_CP_PARAMS="-o $object_id $src_file -s $block_size -c $block_count"
	CLOVIS_CAT_PARAMS="-o $object_id -s $block_size -c $block_count"

	echo "Cancel during write params"
	echo "Object id : $object_id"
	echo "block_size : $block_size"
	echo "block count : $block_count"
	echo "src file : $src_file"
	echo "dest file : $dest_file"

	local cp_cmd="$clovis_st_util_dir/c0cp $CLOVIS_PARAMS $CLOVIS_CP_PARAMS  \
	&> $CLOVIS_TRACE_DIR/c0cp.log  &"

	echo "Executing command: $cp_cmd"
	date
	eval $cp_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops"
	sleep 3

	echo "Sending cancel fop"
	clovis_cancel_session_fop

	echo "Wait for c0cp: $pid to be finished "
	wait $pid
	echo "Copy operation complete"

	# Check for session cancelled messages in c0cp logs
	echo "Check for session cancelled message"
	rc=`cat $CLOVIS_TRACE_DIR/c0cp.log | grep 'Cancelled session' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during write operation"
	else
		# Probably c0cp operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during write operation"
		return 1
	fi

	# If session cancelled successfully during write, then same object
	# read should not be same as src file.
	echo "Check for file difference "
	local cat_cmd="$clovis_st_util_dir/c0cat $CLOVIS_PARAMS $CLOVIS_CAT_PARAMS > $dest_file  "
	eval $cat_cmd

	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Files are not equal"
	else
		# Probably c0cp operation completed before cancel event passed
		# And hence no difference between src and dest file
		echo "Files are equal, means no fops cancelled during write"
		return 1
	fi

}


clovis_cancel_during_read()
{
	object_id=$1;
	block_size=$2;
	block_count=$3;
	src_file=$4;
	dest_file=$5;
	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT  \
	-P $CLOVIS_PROC_FID"
	CLOVIS_CP_PARAMS="-o $object_id $src_file -s $block_size -c $block_count"
	CLOVIS_CAT_PARAMS="-o $object_id -s $block_size -c $block_count"

	echo "Cancel during read params"
	echo "Object id : $object_id"
	echo "block_size : $block_size"
	echo "block count : $block_count"
	echo "src file : $src_file"
	echo "dest file : $dest_file"

	echo "Write the object first before reading it"
	local cp_cmd="$clovis_st_util_dir/c0cp $CLOVIS_PARAMS $CLOVIS_CP_PARAMS  \
	&> $CLOVIS_TRACE_DIR/c0cp.log  &"

	echo "Executing command: $cp_cmd"
	date
	eval $cp_cmd
	pid=$!

	echo "Wait for c0cp: $pid to be finished "
	wait $pid
	echo "Copy operation complete"

	echo "Read the object with object id $object_id"
	local cat_cmd="$clovis_st_util_dir/c0cat $CLOVIS_PARAMS $CLOVIS_CAT_PARAMS > $dest_file 2> $CLOVIS_TRACE_DIR/c0cat.log &"
	eval $cat_cmd
	pid=$!
	date
	echo "Sleep for few seconds to generate enough fops"
	sleep 2

	echo "Sending cancel fop"
	clovis_cancel_session_fop

	echo "Wait for c0cat:$pid to finish"
	wait $pid
	echo "Cat operation is completed "
	date

	# Check for session cancelled messages into c0cat logs
	echo "Check for session cancelled message"
	rc=`cat $CLOVIS_TRACE_DIR/c0cat.log | grep 'Cancelled session' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during read operation"
	else
		# Probably c0cat operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during read operation"
		return 1
	fi

	# If session cancelled successfully during read, then same object
	# read should not be same as src file.
	echo "Check for difference between src file and dest file"
	diff $src_file $dest_file
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Files are not equal"
	else
		# Probably c0cat operation completed before cancel event passed
		# And hence no difference between src and dest file
		echo "Files are equal, means no fops cancelled during write"
		return 1
	fi
}

clovis_cancel_during_create()
{
	object_id=$1;
	n_obj=$2;

	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT  \
	-P $CLOVIS_PROC_FID"

	echo "Cancel during create parameters"
	echo "Object id : $object_id"
	echo "no of object : $n_obj"

	local touch_cmd="$clovis_st_util_dir/c0touch $CLOVIS_PARAMS -o $object_id -n $n_obj &> $CLOVIS_TRACE_DIR/c0touch.log &"
	echo "Command : $touch_cmd"
	date
	eval $touch_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops "
	sleep 3

	echo "Sending cancle fop"
	clovis_cancel_session_fop

	echo "Wait for c0touch: $pid to finished "
	wait $pid
	date
	echo "Done"

	# Check for session cancelled messages into c0touch logs
	echo "Check for session cancelled message"
	rc=`cat $CLOVIS_TRACE_DIR/c0touch.log | grep 'Cancelled session' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during create operation"
	else
		# Probably c0touch operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during create operation"
		return 1
	fi

	return 0
}


clovis_cancel_during_unlink()
{
	object_id=$1;
	n_obj=$2;

	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT  \
	-P $CLOVIS_PROC_FID"

	echo "Cancel during unlink parameters"
	echo "Object id : $object_id"
	echo "no of object : $n_obj"

	# Create objects first to unlink
	local touch_cmd="$clovis_st_util_dir/c0touch $CLOVIS_PARAMS -o $object_id -n $n_obj &> $CLOVIS_TRACE_DIR/c0touch.log &"
	echo "Command : $touch_cmd"
	date
	eval $touch_cmd
	pid=$!

	echo "Wait for c0touch: $pid to finished "
	wait $pid
	date
	echo "Create Done"

	sleep 10
	local unlink_cmd="$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id -n $n_obj &> $CLOVIS_TRACE_DIR/c0unlink.log &"
	echo "Command : $unlink_cmd"
	date
	eval $unlink_cmd
	pid=$!

	echo "Wait for few seconds to generate enough fops "
	sleep 15

	echo "Sending cancle fop"
	clovis_cancel_session_fop

	echo "Wait for c0unlink: $pid to finished "
	wait $pid
	date
	echo "Unlink Done"

	# Check for session cancelled messages into c0unlink logs
	echo "Check for session cancelled message"
	rc=`cat $CLOVIS_TRACE_DIR/c0unlink.log | grep 'Cancelled session' | grep -v grep | wc -l`
	if [ $rc -ne "0" ]
	then
		echo "Cancelled $rc fops during unlink operation"
	else
		# Probably c0unlink operation completed before cancel event passed
		# Hence no fops cancelled
		echo "No fops cancelled during unlink operation"
		return 1
	fi

	return 0
}

main()
{
	clovis_dgmode_sandbox="$CLOVIS_TEST_DIR/sandbox"
	src_file="$CLOVIS_TEST_DIR/src_file"
	dest_file1="$CLOVIS_TEST_DIR/dest_file1"
	dest_file2="$CLOVIS_TEST_DIR/dest_file2"
	object_id1=1048580
	object_id2=1048581
	object_id3=1048587
	block_size=8192
	block_count=16384
	n_obj=400	# No of objects to create/unlink
	n_obj2=200	# No of objects to create/unlink

	sandbox_init

	echo "Creating src file"
	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
	      2> $CLOVIS_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	echo "Make clovis test directory $CLOVIS_TRACE_DIR"
	mkdir $CLOVIS_TRACE_DIR

	echo "Starting mero services"
	mero_service_start $N $K $P $stride

	echo "=========================================================="
	echo "TC1. Clovis RPC cancel during clovis write."
	echo "=========================================================="
	clovis_cancel_during_write $object_id1 $block_size $block_count \
		$src_file $dest_file1
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop mero services and exit"
		mero_service_stop
		sandbox_fini
		return 1
	fi
	echo "=========================================================="
	echo "TC1. Clovis cancel during clovis write complete."
	echo "=========================================================="


	echo "=========================================================="
	echo "TC2. Clovis RPC cancel during clovis read."
	echo "=========================================================="
	clovis_cancel_during_read $object_id2 $block_size $block_count \
		$src_file $dest_file2
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop mero services and exit"
		mero_service_stop
		sandbox_fini
		return 1
	fi
	echo "=========================================================="
	echo "TC2. Clovis cancel during clovis read complete."
	echo "=========================================================="

	echo "=========================================================="
	echo "TC3. Clovis RPC cancel during clovis create."
	echo "=========================================================="
	clovis_cancel_during_create $object_id3 $n_obj
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop mero services and exit"
		mero_service_stop
		sandbox_fini
		return 1
	fi
	echo "=========================================================="
	echo "TC3. Clovis cancel during clovis create complete."
	echo "=========================================================="

	echo "=========================================================="
	echo "TC4. Clovis RPC cancel during clovis unlink."
	echo "=========================================================="
	clovis_cancel_during_unlink $object_id2 $n_obj2
	if [ $? -ne "0" ]
	then
		echo "Failed to run the test, Stop mero services and exit"
		mero_service_stop
		sandbox_fini
		return 1
	fi
	echo "=========================================================="
	echo "TC4. Clovis cancel during clovis unlink complete."
	echo "=========================================================="

	echo "Stopping Mero services"
	mero_service_stop

	sandbox_fini
	return 0;
}

echo "clovis RPC cancel test..."
main
report_and_exit clovis-rpc-cancel $?
