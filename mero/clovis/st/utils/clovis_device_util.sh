#!/bin/bash

# Script for starting Clovis system tests in "scripts/m0 run-st"

#set -x

clovis_st_util_dir=`dirname $0`
m0t1fs_st_dir=$clovis_st_util_dir/../../../m0t1fs/linux_kernel/st

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_dir/common.sh
. $m0t1fs_dir/m0t1fs_common_inc.sh
. $m0t1fs_dir/m0t1fs_client_inc.sh
. $m0t1fs_dir/m0t1fs_server_inc.sh
. $m0t1fs_dir/m0t1fs_sns_common_inc.sh

clovis_st_set_failed_devices()
{
	disk_state_set "failed" $1 || {
		echo "Failed: pool_mach_set_failure..."
		return 1
	}
}

clovis_st_query_devices()
{
	disk_state_get $1 || {
		echo "Failed: pool_mach_query..."
		return 1
	}
}

# Print out usage
usage()
{
	cat <<.
Usage:

$ sudo clovis_device_util [down|query] devices
.
}

# Get options
cmd=$1
devices=$2

case "$cmd" in
	down)
		clovis_st_set_failed_devices $devices
		;;
	query)
		clovis_st_query_devices $devices
		;;
	*)
		usage
		exit
esac


# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
