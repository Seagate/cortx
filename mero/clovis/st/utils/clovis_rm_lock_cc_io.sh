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

#Checking the version of file read to be in non-decreasing order.
check_file_version()
{
	i=1
	j=1

	while (( j<=$reader_numb ))
	do
		if (( i>$writer_numb ))
		then
			return 1
		fi

		diff $dest_file$j $src_file$i > /dev/null || {
			((i++))
			continue
		}
		((j++))
	done

	return 0
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	clovis_dgmode_sandbox="$CLOVIS_TEST_DIR/sandbox"
	src_file="$CLOVIS_TEST_DIR/src_file"
	dest_file="$CLOVIS_TEST_DIR/dest_file"
	object_id=1048580
	block_size=4096
	block_count=2048
	writer_numb=5
	reader_numb=5
	SRC_FILES=""
	DEST_FILES=""
	CLOVIS_PARAMS="-l $CLOVIS_LOCAL_EP -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT \
                       -P $CLOVIS_PROC_FID"
	CLOVIS_PARAMS_2="-l $CLOVIS_LOCAL_EP2 -H $CLOVIS_HA_EP -p $CLOVIS_PROF_OPT \
                         -P $CLOVIS_PROC_FID"

	rm -f $src_file $dest_file

	dd if=/dev/urandom bs=$block_size count=$block_count of=$src_file \
           2> $CLOVIS_TEST_LOGFILE || {
		clovis_error_handling $? "Failed to create a source file"
	}
	mkdir $CLOVIS_TRACE_DIR

	for (( i=1; i<=$writer_numb; i++ ))
	do
		tmp_src="$src_file$i"
		rm -f $tmp_src
		dd if=/dev/urandom bs=$block_size count=$block_count of=$tmp_src \
                   2> $CLOVIS_TEST_LOGFILE || {
			clovis_error_handling $? "Failed to create a source file"
	        }
		SRC_FILES+=" $tmp_src"
	done
        SRC_FILES="$(echo -e "${SRC_FILES}" | sed -e 's/^[[:space:]]*//')"

	for (( j=1; j<=$reader_numb; j++ ))
	do
		tmp_dest="$dest_file$j"
		rm -f $tmp_dest
		DEST_FILES+=" $tmp_dest"
	done
        DEST_FILES="$(echo -e "${DEST_FILES}" | sed -e 's/^[[:space:]]*//')"

	mero_service_start
	dix_init

##############################################################################
	echo "Read obj while write/update is in process."

	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_2 -o $object_id $src_file \
                                 -s $block_size -c $block_count -e &
	pid=$!
	sleep 2

	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS -o $object_id -s $block_size \
                                  -c $block_count -e $dest_file || {
		clovis_error_handling $? "Failed to read object"
	}

	wait $pid
	diff $src_file $dest_file || {
		rc = $?
		clovis_error_handling $rc "Files are different when concurrent read/write"
	}

	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id || {
		clovis_error_handling $? "Failed to delete object"
	}

	rm -f $dest_file
###############################################################################
	echo "Delete obj while write/update is in process."

	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS_2 -o $object_id $src_file \
                                 -s $block_size -c $block_count -e &

	pid=$!
	sleep 2

	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id -e || {
		clovis_error_handling $? "Failed to delete object"
	}
	wait $pid

###############################################################################
	echo "Delete obj while read is in process."

	$clovis_st_util_dir/c0cp $CLOVIS_PARAMS -o $object_id $src_file \
                                -s $block_size -c $block_count -e || {
		clovis_error_handling $? "Failed to copy object"
	}

	$clovis_st_util_dir/c0cat $CLOVIS_PARAMS_2 -o $object_id -s $block_size \
                                 -c $block_count -e $dest_file &
	pid=$!
	sleep 2

	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id -e || {
		clovis_error_handling $? "Failed to delete object"
	}

	wait $pid
	diff $src_file $dest_file || {
		rc = $?
		clovis_error_handling $rc "Files are different when concurrent delete/write"
	}

	rm -f $dest_file
#############################################################################
	echo "Launch multiple Writer and Reader threads."
	echo "To check the data read by reader threads is fresh."
	$clovis_st_util_dir/c0cc_cp_cat $CLOVIS_PARAMS -W $writer_numb \
					-R $reader_numb -o $object_id \
					-s $block_size -c $block_count \
					$SRC_FILES $DEST_FILES || {
		clovis_error_handling $? "Failed concurrent read write"
	}

	$clovis_st_util_dir/c0unlink $CLOVIS_PARAMS -o $object_id -e || {
		clovis_error_handling $? "Failed to delete object"
	}

	echo "Checking file versions for stale data."
	check_file_version || {
		clovis_error_handling $? "Stale data read"
	}

	clean &>>$CLOVIS_TEST_LOGFILE
	mero_service_stop

}

echo "Clovis RM lock CC_IO Test ... "
main
report_and_exit clovis_rm_lock_cc_io_st $?
