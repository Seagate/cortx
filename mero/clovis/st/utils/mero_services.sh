#!/bin/bash

# main entry
#set -x

CLOVIS_ST_UTIL_DIR=`dirname $0`

if [ ${0:0:1} = "/" ]; then
	CLOVIS_ST_UTIL_DIR=`dirname $0`
else
	CLOVIS_ST_UTIL_DIR=$PWD/`dirname $0`
fi
MERO_DIR=${CLOVIS_ST_UTIL_DIR%/clovis*}
echo " MERO_DIR =$MERO_DIR"

M0T1FS_ST_DIR=$MERO_DIR/m0t1fs/linux_kernel/st
echo "$M0T1FS_ST_DIR"

. $M0T1FS_ST_DIR/common.sh
. $M0T1FS_ST_DIR/m0t1fs_common_inc.sh
. $M0T1FS_ST_DIR/m0t1fs_client_inc.sh
. $M0T1FS_ST_DIR/m0t1fs_server_inc.sh


# start | stop service
multiple_pools_flag=1
case "$1" in
    start)
	mero_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Mero Service."
	fi
	;;
    stop)
	mero_service stop $multiple_pools_flag
	echo "Mero services stopped."
	;;
    *)
	echo "Usage: $0 {start|stop]}"
esac

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
