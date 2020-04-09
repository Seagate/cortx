#!/bin/bash

# Script for starting or stopping Clovis system tests

. `dirname $0`/clovis_st_inc.sh
# enable core dumps
ulimit -c unlimited

# Debugger to use
debugger=

# Print out usage
usage()
{
	cat <<.
Usage:

$ sudo clovis_sys_test [start|stop|list|run] [local|remote] [-d debugger] \
[-i Index-service] [-t tests] [-k] [-u]

Where:

start: starts only clovis system tests.
stop : stops clovis system tests.
list : Lists all the available clovis system tests.
run  : Starts Mero services, executes clovis system tests and then\
stops mero services.

-d: Invoke a debugger if set, only gdb is supported currently

-i: Select Index service:
    CASS : Cassandra
    KVS  : Mero KVS

-k: run Clovis system tests in kernel mode

-u: run Clovis system tests in user space mode

-t TESTS: Only run selected tests

-r: run tests in a suite in random order
.
}
OPTIONS_STRING="d:i:kurt:"

# Get options
cmd=$1
conf=$2
shift 2

umod=1
random_mode=0
while getopts "$OPTIONS_STRING" OPTION; do
	case "$OPTION" in
		d)
			debugger="$OPTARG"
			;;
		i)
			index="$OPTARG"
			;;
		k)
			umod=0
			echo "Running Clovis ST in Kernel mode"
			;;
		u)
			umod=1
			echo "Running Clovis ST in User mode"
			;;
		t)
			tests="$OPTARG"
			;;
		r)
			random_mode=1
			;;
		*)
			usage
			exit 1
			;;
	esac
done

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

case "$cmd" in
	start)
		sandbox_init

		if [ $umod -eq 1 ]; then
			clovis_st_start_u $index $debugger
		else
			clovis_st_start_k $index
		fi

		rc=$?
		sandbox_fini $rc
		report_and_exit clovis_sys_test $rc
		;;
	run)
		( exec `dirname $0`/mero_services.sh start )
		sandbox_init

		if [ $umod -eq 1 ]; then
			clovis_st_start_u $index $debugger
		else
			clovis_st_start_k $index
		fi

		rc=$?
		sandbox_fini $rc
		( exec `dirname $0`/mero_services.sh stop )
		report_and_exit clovis_sys_test $rc
		;;
	stop)
		if [ $umod -eq 1 ]; then
			clovis_st_stop_u
		else
			clovis_st_stop_k
		fi
		;;
	list)
		sandbox_init
		clovis_st_list_tests
		sandbox_fini 0
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
