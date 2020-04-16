#!/bin/bash

#set -x

clovis_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )
mero_dir="$clovis_st_util_dir/../../.."
m0t1fs_dir="$mero_dir/m0t1fs/linux_kernel/st"

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $m0t1fs_dir/m0t1fs_sns_common_inc.sh

. $clovis_st_util_dir/clovis_local_conf.sh
. $clovis_st_util_dir/clovis_st_inc.sh

mero_sandbox=$SANDBOX_DIR
m0crate_trace_dir=$mero_sandbox/clovis
m0crate_logfile=$m0crate_trace_dir/m0crate_`date +"%Y-%m-%d_%T"`.log
m0crate_workload_yaml=$clovis_st_util_dir/m0crate_st_workloads.yaml

m0crate_src_size=16 # in MB
m0crate_src_file=$mero_sandbox/m0crate_"$m0crate_src_size"MB

customise_mero_configs()
{
	local yaml=$m0crate_workload_yaml

	echo $yaml
	sed -i "s/^\([[:space:]]*MERO_LOCAL_ADDR: *\).*/\1$CLOVIS_LOCAL_EP/" $yaml
	sed -i "s/^\([[:space:]]*MERO_HA_ADDR: *\).*/\1$CLOVIS_HA_EP/" $yaml
	sed -i "s/^\([[:space:]]*CLOVIS_PROF: *\).*/\1$CLOVIS_PROF_OPT/" $yaml
	sed -i "s/^\([[:space:]]*CLOVIS_PROCESS_FID: *\).*/\1$CLOVIS_PROC_FID/" $yaml
	sed -i "s#^\([[:space:]]*SOURCE_FILE: *\).*#\1$m0crate_src_file#" $yaml
}

run_m0crate()
{
	local cmd=$mero_dir/clovis/m0crate/m0crate
	local cmd_arg="-S $m0crate_workload_yaml"

	if [ ! -f $cmd ] ; then
		echo "Can't find m0crate at $cmd"
		return 1
	fi

	local cwd=`pwd`
	cd $m0crate_trace_dir
	eval $cmd $cmd_arg &
	wait $!
	if [ $? -ne 0 ]
	then
		echo "  Failed to run command $cmd $cmd_arg"
		cd $cwd
		return 1
	fi
	cd $cwd
	return 0
}

main()
{

	sandbox_init

	NODE_UUID=`uuidgen`
	mkdir $m0crate_trace_dir || {
		echo "Failed to create trace directory"
		return 1
	}
	customise_mero_configs $m0crate_io_yaml
	rc=0

	# Start mero services.
	mero_service_start
	dix_init

	# Prepare and and run.
	block_size=4096
	block_count=`expr $m0crate_src_size \* 1024 \* 1024 / $block_size`
	echo "dd if=/dev/urandom bs=$block_size count=$block_count of=$m0crate_src_file"
	dd if=/dev/urandom bs=$block_size count=$block_count of=$m0crate_src_file 2> $m0crate_logfile || {
		echo "Failed to create a source file"
		mero_service_stop
		return 1
	}

	run_m0crate || {
		rc=$?
		echo "m0crate IO workload failed."
		error_handling $rc
	}

	# Stop services and clean up.
	mero_service_stop || rc=1
	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		error_handling $rc
	fi
	return $rc
}

echo "m0crate TEST ... "
trap unprepare EXIT
main
report_and_exit m0crate $?
