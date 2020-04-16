#!/bin/bash

# Script to start Clovis CmdLine tool (m0clovis).

usage()
{
	cat <<.
Usage:

$ sudo m0clovis_start.sh [local|remote] [-v] ["(m0clovis index commands)"]
.
}

conf=$1
shift 1
verbose=0
if [ $1 == "-v" ] ; then
	verbose=1
	shift
fi
all=$*

function m0clovis_cmd_start()
{
	# Assembly command
	local exec="`dirname $0`/../../m0clovis/m0clovis"
	if [ ! -f $exec ];then
		echo "Can't find m0clovis"
		return 1
	fi

	local args="-l $CLOVIS_LOCAL_EP -h $CLOVIS_HA_EP \
		    -p '$CLOVIS_PROF_OPT' -f '$CLOVIS_PROC_FID'"
	local cmdline="$exec $args $all"
	if [ $verbose == 1 ]; then
		echo "Running m0clovis command line tool..."
		echo "$cmdline" > /dev/tty
	fi
	eval $cmdline || {
		err=$?
		echo "Clovis CmdLine utility returned $err!"
		return $err
	}

	return $?
}

case "$conf" in
	local)
		. `dirname $0`/clovis_local_conf.sh
		;;
	remote)
		. `dirname $0`/clovis_remote_conf.sh
		;;
	*)
		usage
		exit
esac

m0clovis_cmd_start
exit $?

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
