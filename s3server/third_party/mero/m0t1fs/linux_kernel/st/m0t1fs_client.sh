#!/usr/bin/env bash

usage()
{
	echo "Usage: `basename $0` server_nid"
	echo "Please provide the server nid you want to use."
	echo "e.g. 192.168.172.130@tcp"
}

if [ $# -lt 1 ]
then
	usage
        exit 1
fi

if [ "x$1" = "x-h" ];
then
	usage
	exit 0
fi

server_nid=$1

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh

main()
{
	prepare

	echo "Prepare done, starting tests..."

	m0t1fs_system_tests
	if [ $? -ne "0" ]
	then
		return 1
	fi

        return 0
}

trap unprepare EXIT

main $1
