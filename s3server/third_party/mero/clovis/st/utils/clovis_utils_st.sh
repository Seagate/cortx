#!/bin/bash

clovis_st_util_dir=$(dirname $(readlink -f $0))
m0t1fs_dir="$clovis_st_util_dir/../../../m0t1fs/linux_kernel/st"

. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $clovis_st_util_dir/clovis_local_conf.sh
. $clovis_st_util_dir/clovis_st_inc.sh


SANDBOX_DIR=/var/mero
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

test_with_N_K()
{
	src_file="$CLOVIS_TEST_DIR/src_file"
	src_file_extra="$CLOVIS_TEST_DIR/src_file_extra"
	dest_file="$CLOVIS_TEST_DIR/dest_file"
	object_id1=0x7300000000000001:0x32
	object_id2=0x7300000000000001:0x33
	object_id3=0x7300000000000001:0x34
	object_id4=1048577
	block_size=4096
	block_count=5120
	obj_count=5
	trunc_len=2560
	trunc_count=17
	read_verify="false"
	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT \
                       -P $CLOVIS_PROC_FID"
	CLOVIS_PARAMS_V="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT \
                         -P $CLOVIS_PROC_FID"
	if [[ $read_verify == "true" ]]; then
		CLOVIS_PARAMS_V+=" -r"
	fi
	rm -f $src_file
	local source_abcd=$CLOVIS_TEST_DIR/"abcd"
	dd if=$source_abcd bs=$block_size count=$block_count of=$src_file \
           2> $CLOVIS_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	dd if=$source_abcd bs=$block_size \
	   count=$(($block_count + $trunc_count)) of=$src_file_extra \
	   2> $CLOVIS_TEST_LOGFILE || {
		error_handling $? "Failed to create a source file"
	}
	echo "count: $count"

	N=$1
	K=$2
	P=$3
	stride=32

	mero_service_start $N $K $P $stride
	dix_init

	# Test c0client utility
	/usr/bin/expect <<EOF
	set timeout 20
	spawn $clovis_st_util_dir/c0client $CLOVIS_PARAMS_V > $SANDBOX_DIR/c0client.log
	expect "c0clovis >>"
	send -- "touch $object_id3\r"
	expect "c0clovis >>"
	send -- "write $object_id2 $src_file $block_size $block_count\r"
	expect "c0clovis >>"
	send -- "read $object_id2 $dest_file $block_size $block_count\r"
	expect "c0clovis >>"
	send -- "delete $object_id3\r"
	expect "c0clovis >>"
	send -- "delete $object_id2\r"
	expect "c0clovis >>"
	send -- "quit\r"
EOF
	echo "c0client test is Successful"
	rm -f $dest_file

	echo "c0touch and c0unlink"
	$clovis_st_util_dir/c0touch $CLOVIS_PARAMS -o $object_id1 -L 9|| {
		error_handling $? "Failed to create a object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	echo "c0touch and c0unlink successful"

	$clovis_st_util_dir/c0touch $CLOVIS_PARAMS -o $object_id1 -L 9|| {
		error_handling $? "Failed to create a object"
	}

	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 -u|| {
		error_handling $? "Failed to copy object"
	}
	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9\
				  > $dest_file || {
		error_handling $? "Failed to read object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	diff $src_file $dest_file || {
		rc=$?
		error_handling $rc "Files are different"
	}
	echo "clovis r/w test with c0cp and c0cat is successful"
	rm -f $dest_file

	# Test c0cp_mt
	echo "c0cp_mt test"
	$clovis_st_util_dir/c0cp_mt $CLOVIS_PARAMS_V -o $object_id4 \
				    -n $obj_count $src_file -s $block_size \
				    -c $block_count -L 9 || {
		error_handling $? "Failed to copy object"
	}
	for i in $(seq 0 $(($obj_count - 1)))
	do
		object_id=$(($object_id4 + $i));
		$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_V -o $object_id \
					  -s $block_size -c $block_count -L 9\
					  > $dest_file || {
			error_handling $? "Failed to read object"
		}
		diff $src_file $dest_file || {
			rc=$?
			error_handling $rc "Files are different"
		}
		rm -f $dest_file
	done
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id4 \
				     -n $obj_count || {
		error_handling $? "Failed to delete object"
	}
	echo "c0cp_mt is successful"

	# Test truncate/punch utility
	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9|| {
		error_handling $? "Failed to copy object"
	}
	$clovis_st_util_dir/c0trunc $CLOVIS_PARAMS -o $object_id1 \
				    -c $trunc_count -t $trunc_len \
				    -s $block_size -L 9 || {
		error_handling $? "Failed to truncate object"
	}
	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9\
				  > $dest_file-full || {
		error_handling $? "Failed to read object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	cp $src_file $src_file-punch
	fallocate -p -o $(($trunc_count * $block_size)) \
		  -l $(($trunc_len * $block_size)) -n $src_file-punch
	diff -q $src_file-punch $dest_file-full || {
		rc=$?
		error_handling $rc "Punched Files are different"
	}
	echo "C0trunc: Punching hole is successful"
	rm -f $src_file-punch $dest_file-full

	# Truncate file to zero
	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9 || {
		error_handling $? "Failed to copy object"
	}
	$clovis_st_util_dir/c0trunc $CLOVIS_PARAMS -o $object_id1 -c 0 \
                                   -t $block_count -s $block_size -L 9|| {
		error_handling $? "Failed to truncate object"
	}
	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_V -o $object_id1 \
				  -s $block_size -c $block_count -L 9 \
				  > $dest_file || {
		error_handling $? "Failed to read from truncated object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	cp $src_file $src_file-trunc
	fallocate -p -o 0 -l $(($block_count * $block_size)) -n $src_file-trunc
	diff -q $src_file-trunc $dest_file || {
		rc=$?
		error_handling $rc "Truncated Files are different"
	}
	echo "c0trunc: Truncate file to zero is successful"
	rm -f $src_file-trunc $dest_file

	# Truncate range beyond EOF
	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_V -o $object_id1 $src_file \
                                 -s $block_size -c $block_count -L 9|| {
		error_handling $? "Failed to copy object"
	}
	$clovis_st_util_dir/c0trunc $CLOVIS_PARAMS -o $object_id1 \
				    -c $trunc_count -t $block_count \
				    -s $block_size -L 9|| {
		error_handling $? "Failed to truncate object"
	}
	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_V -o $object_id1 \
				  -s $block_size \
				  -c $(($block_count + $trunc_count)) -L 9\
				  > $dest_file || {
		error_handling $? "Failed to read from truncated object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	fallocate -p -o $(($trunc_count * $block_size)) \
		  -l $(($block_count  * $block_size)) -n $src_file_extra
	diff -q $src_file_extra $dest_file || {
		rc=$?
		error_handling $rc "Truncat Files beyond EOF are different"
	}
	echo "c0trunc: Truncate range beyond EOF is successful"
	rm -f $src_file_extra $dest_file

	# Truncate a zero size object
	$clovis_st_util_dir/c0touch $CLOVIS_PARAMS -o $object_id1 -L 9|| {
		error_handling $? "Failed to create a object"
	}
	$clovis_st_util_dir/c0trunc $CLOVIS_PARAMS -o $object_id1 -c 0 \
				    -t $block_count -s $block_size -L 9|| {
		error_handling $? "Failed to truncate object"
	}
	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id1 || {
		error_handling $? "Failed to delete object"
	}
	echo "c0trunc: Truncate zero size object successful"

	rm -f $src_file
	clean &>>$CLOVIS_TEST_LOGFILE
	mero_service_stop
	return 0
}

main()
{
	sandbox_init

	rc=0
	NODE_UUID=`uuidgen`
	clovis_dgmode_sandbox="$CLOVIS_TEST_DIR/sandbox"

	rm -f $source_abcd
	st_dir=$M0_SRC_DIR/m0t1fs/linux_kernel/st
	prog_file_pattern="$st_dir/m0t1fs_io_file_pattern"

	local source_abcd="abcd"
	echo "Creating data file $source_abcd"
	$prog_file_pattern $source_abcd || {
		echo "Failed: $prog_file_pattern"
		error_handling $? "Failed to copy object"
	}
	mkdir $CLOVIS_TRACE_DIR
	P=8
	N=1
	K=0
	test_with_N_K $N $K $P
	if [ $rc -ne "0" ]
	then
		echo "Clovis util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Clovis util test with N=$N K=$K is successful"

	N=1
	K=2
	test_with_N_K $N $K $P
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Clovis util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Clovis util test with N=$N K=$K is successful"

	N=4
	K=2
	test_with_N_K $N $K $P
	rc=$?
	echo $rc
	if [ $rc -ne "0" ]
	then
		echo "Clovis util test with N=$N K=$K failed"
		return $rc
	fi
	echo "Clovis util test with N=$N K=$K is successful"

	rm -f $source_abcd
	sandbox_fini
	return $rc
}

echo "Clovis Utils Test ... "
main
report_and_exit clovis-utils-st $?
