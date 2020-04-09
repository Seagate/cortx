#!/usr/bin/env bash
# set -eux
# export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

# Note: only 1 client/1 server configuration supported in this script.
# Update console_cmdline_complement() calculations for other configurations.

# Note: m0.trace.* files will be created in user home directory on
# each host in test. Large number of these files can lead to
# "no free disk space" problem. Possible solution: remove these files
# after every test run. See TRACE_RM variable in $SCRIPT_TEST_RUN

# Directory structure
DIR_SCRIPT="$(readlink -f ${0%/*})"
#	directory with this script
DIR_SETUP="$DIR_SCRIPT/setup"
#	[clients|servers]-[user|kernel]
#		command line and kernel module parameters for
#		the test clients/servers
#	[clients|servers|console]-NIDs-raw
#		LNET NID list for each client/server
#	concurrency
#	msg_size
#		list of possible values of concurrency and message sizes
DIR_DATA="$DIR_SCRIPT/data"
#	[bulk|ping]-[kernel|user]-[kernel|user]/{concurrency}-{msg_size}-raw
#		console output for test with given concurrency (value) and
#		message size (value). First [kernel|user] is for test clients,
#		second is for test server.
DIR_RESULT="$DIR_SCRIPT/result"
#	tables and graphs are here
DIR_TABLES="$DIR_RESULT/tables"
#	contains tables with ticks for both axis
#	0X - test message size
#	0Y - test concurrency
DIR_TABLES_UNIFORM="$DIR_RESULT/tables-uniform"
#	contains tables without axis ticks
# DIR_TABLES_*/
#	[bulk|ping]-[kernel|user]-[kernel|user]-\
#		[Bandwidth|RTT|MPS]-[min|max|avg|stddev]
#	Table with aggregated measurement results for corresponding tests.
DIR_PLOT_SCRIPTS="$DIR_RESULT/gnuplot-scripts"
#	gnuplot scripts to produce graphs

# Parameters
# Verbose mode
VERBOSE=0

# Resume execution after ^C interrupt
RESUME=0
RESUME2=0

# Hardcoded in script
# Transfer machine ID in test will start from this number
TMID_START=300
# Maximum number of test nodes (for demo)
NODES_NR_MAX=64
# Time limit for one test, seconds
TEST_RUN_TIME=5
# Message number limit for one test
# Test will be finished if MSG_NR messages sent/received or time limit reached
MSG_NR=10000000

# Size for msg buffer with bulk network buffer descriptors
BD_BUF_SIZE=16k
# Maximum number of descriptors in this buffer
BD_NR_MAX=16384

# LNET Network Type. First NID from `lctl network list_nids' with
# $NET_TYPE network type are used for testing.
NET_TYPE=o2ib
# NET_TYPE=tcp
# NET_TYPE=lo
LNET_PID=12345
LNET_PORTAL=42
# Maximum network speed for IB switch. Used in graphs. Info from 'lspci':
# 08:00.0 InfiniBand: Mellanox Technologies MT26428
# [ConnectX VPI PCIe 2.0 5GT/s - IB QDR / 10GigE] (rev b0)
NET_BANDWIDTH_MAX=$(expr 4 "*" 1024 "*" 1024 "*" 1024)

# User-supplied list of ssh credentials for test nodes and test console
declare -a CLIENTS
declare -a SERVERS
CONSOLE=

# Configuration for test nodes
declare -A NODES
NODES_NR=0

# These parameters will be calculated from others
# See console_cmdline_complement()
CONCURRENCY_CLIENT=
CONCURRENCY_SERVER=
BD_BUF_NR_CLIENT=
BD_BUF_NR_SERVER=

# List of parameters to test
MSG_SIZE_LIST="64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 \
262144 524288 1048576"
CONCURRENCY_CLIENT_LIST="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 18 20 22 24 \
26 28 30 32 40 48 56 64 80 96 112 128 160 192 224 256"
MEASUREMENT_LIST="Bandwidth RTT MPS"
M_TYPE_LIST="min max avg stddev"
declare -a MSG_SIZE_ARR=(${MSG_SIZE_LIST// / })
declare -a CONCURRENCY_CLIENT_ARR=(${CONCURRENCY_CLIENT_LIST// / })
declare -a MEASUREMENT_ARR=(${MEASUREMENT_LIST// / })
declare -a M_TYPE_ARR=(${M_TYPE_LIST// / })
declare -a DIR_NAME_ARR=
declare -A MSG_SIZE_REVERSE

# Used by script
FILE_CONSOLE_NID="$DIR_SETUP/console-NIDs-raw"
FILE_CLIENTS_NID="$DIR_SETUP/clients-NIDs-raw"
FILE_SERVERS_NID="$DIR_SETUP/servers-NIDs-raw"
FILE_CONSOLE_CMD="$DIR_SETUP/console-user"
FILE_CLIENTS_CMD="$DIR_SETUP/clients"
FILE_SERVERS_CMD="$DIR_SETUP/servers"
FILE_MSG_SIZE="$DIR_SETUP/msg_size"
FILE_CONCURRENCY="$DIR_SETUP/concurrency"

SCRIPT_CONFIG="$DIR_SCRIPT/demo-config.sh"
SCRIPT_LIST_NIDS="$DIR_SCRIPT/demo-list-nids.sh"
SCRIPT_TEST_RUN="$DIR_SCRIPT/demo-test-run.sh"

CONSOLE_TMID_CLIENTS=$TMID_START
CONSOLE_TMID_SERVERS=$(expr $TMID_START + 1)
NODES_TMID_START=$(expr $CONSOLE_TMID_SERVERS + 1)

CONSOLE_NID=
CONSOLE_EP_CLIENTS=
CONSOLE_EP_SERVERS=
CONSOLE_CMDLINE=

MKDIR="mkdir -p"
ECHO="echo -e"
ECHO_N="echo -n"
RM_RF="rm -rf"
# Tested with v4.6.1
# devvm has v4.2.6 and v4.4.2 - it will not work
GNUPLOT="gnuplot"

main()
{
	. "$SCRIPT_CONFIG"
	script_init_once
	script_init
	cmdline_parse "$@"
	if [ $RESUME -eq 0 ]; then
		configuration_parse
		dirs_create
		configure
	else
		config_load
		script_init
		dirs_create
	fi
	if [ $RESUME2 -eq 0 ]; then
		for_each_combination dirs_make
		for_each_combination test_run
		for_each_table table_set0
		for_each_combination console_output_parse
	fi
	draw_graphs
}

cmdline_help()
{
	$ECHO "-?"
	$ECHO "-h\tPrint help and exit"
	$ECHO "-v\tVerbose output"
	$ECHO "-d\tRemove all produced files and results"
	$ECHO "-c\tComma-separated list of clients"
	$ECHO "-s\tComma-separated list of servers"
	$ECHO "-e\tConsole address"
	$ECHO "-r\tResume script execution after ^C"
	$ECHO "-R\tResume script execution after ^C (draw graphs)"
}

cmdline_parse()
{
	OPTIND=1
	local IFS_SAVE="$IFS"
	while getopts "?hvdc:s:e:rR" opt; do
		case "$opt" in
		h|\?)	cmdline_help
			exit 0 ;;
		v)	VERBOSE=1 ;;
		d)	files_clean
			exit 0 ;;
		c)	IFS=","
			read -a CLIENTS <<< "$OPTARG" ;;
		s)	IFS=","
			read -a SERVERS <<< "$OPTARG" ;;
		e)	CONSOLE="$OPTARG" ;;
		r)	RESUME=1 ;;
		R)	RESUME=1;RESUME2=1 ;;
		esac
	done
	IFS="$IFS_SAVE"
}

script_init()
{
	local msg_size
	local index=1

	for msg_size in ${MSG_SIZE_ARR[@]}; do
		((index++)) || true
		MSG_SIZE_REVERSE["$msg_size"]="$index"
	done
}

script_init_once()
{
	local test_type
	local space_c
	local space_s

	for test_type in "ping" "bulk"; do
		for space_c in "user" "kernel"; do
			for space_s in "user" "kernel"; do
				DIR_NAME_ARR=("${DIR_NAME_ARR[@]}" \
					      "$test_type-$space_c-$space_s")
			done
		done
	done
}

# Setter and getter for node associative array
# $1 - node index in array
# $2 - parameter name
# node_set(): $3 - new value
# node_get(): returns value
node_set()
{
	local index="$1_$2"
	NODES[$index]=$3
}

node_get()
{
	local index="$1_$2"
	echo "${NODES[$index]}"
}

# $1 - error string
# $2 - error code ($?)
exit_if_error()
{
	if [ "$2" -ne 0 ]; then
		echo $1
		perror $2
		exit 1
	fi
}

configuration_parse()
{
	local ssh_addr
	local addr
	local index

	for ssh_addr in "${CLIENTS[@]}" "${SERVERS[@]}"; do
		if [ "$NODES_NR" -eq "$NODES_NR_MAX" ]; then
			echo "Too many nodes; current limit is $NODES_NR_MAX"
			exit 1
		fi
		index="$NODES_NR"
		# configure nodes
		node_set $index "ssh_addr" $ssh_addr
		node_set $index "tmid_cmd" \
			$(expr $NODES_TMID_START + $NODES_NR "*" 2)
		node_set $index "tmid_data" \
			$(expr $NODES_TMID_START + $NODES_NR "*" 2 + 1)
		((NODES_NR++)) || true
	done
	index=0
	for addr in "${CLIENTS[@]}"; do
		node_set "$index" "role" "client"
		((index++)) || true
	done
	for addr in "${SERVERS[@]}"; do
		node_set "$index" "role" "server"
		((index++)) || true
	done
}

# $1 - function to call.
#      Node index will be passed to this function as first argument.
# $2 .. - this parameters will be passed to function as $3 .. .
for_each_node()
{
	local i
	local func="$1"
	shift 1
	for i in $(seq 0 $(expr $NODES_NR - 1)); do
		$func $i $@
	done
}

node_configure()
{
	local index=$1
	local nid=$(node_get $index "nid")
	local tmid=$(node_get $index "tmid_cmd")
	local endpoint_cmd="$nid:$LNET_PID:$LNET_PORTAL:$tmid"
	node_set $index "endpoint_cmd" "$endpoint_cmd"
	local tmid=$(node_get $index "tmid_data")
	node_set $index "endpoint_data" "$nid:$LNET_PID:$LNET_PORTAL:$tmid"
	local role=$(node_get $index "role")
	local endpoint_console
	if [ "$role" == "client" ]; then
		endpoint_console="$CONSOLE_EP_CLIENTS"
	else
		endpoint_console="$CONSOLE_EP_SERVERS"
	fi
	node_set $index "endpoint_console" "$endpoint_console"
	node_set $index "cmdline_user" \
		"$CMD_NODE -a $endpoint_cmd -c $endpoint_console"
	node_set $index "cmdline_kernel" \
	  "$MOD_NODE addr_console=$endpoint_console addr=$endpoint_cmd"
}

console_configure1()
{
	local console_ep_prefix="$CONSOLE_NID:$LNET_PID:$LNET_PORTAL"
	CONSOLE_EP_CLIENTS="$console_ep_prefix:$CONSOLE_TMID_CLIENTS"
	CONSOLE_EP_SERVERS="$console_ep_prefix:$CONSOLE_TMID_SERVERS"
}

add_comma()
{
	if [ "$2" == "" ]; then
		echo "$1"
	else
		echo "$2,$1"
	fi
}

console_configure2()
{
	local clients_cmd=
	local clients_data=
	local servers_cmd=
	local servers_data=
	local index
	local cmd=

	for index in $(seq 0 $(expr $NODES_NR - 1)); do
		local role=$(node_get $index "role")
		local ep_cmd=$(node_get $index "endpoint_cmd")
		local ep_data=$(node_get $index "endpoint_data")
		if [ "$role" == "client" ]; then
			clients_cmd=$(add_comma $ep_cmd "$clients_cmd")
			clients_data=$(add_comma $ep_data "$clients_data")
		else
			servers_cmd=$(add_comma $ep_cmd "$servers_cmd")
			servers_data=$(add_comma $ep_data "$servers_data")
		fi
	done
	cmd="-A $CONSOLE_EP_SERVERS -a $CONSOLE_EP_CLIENTS"
	cmd="$cmd -C $servers_cmd -c $clients_cmd"
	cmd="$cmd -D $servers_data -d $clients_data"
	CONSOLE_CMDLINE="$CMD_CONSOLE $cmd"
}

dirs_create()
{
	$MKDIR "$DIR_SETUP" "$DIR_DATA" "$DIR_RESULT" "$DIR_PLOT_SCRIPTS"
	$MKDIR "$DIR_TABLES" "$DIR_TABLES_UNIFORM"
}

files_clean()
{
	$RM_RF "$DIR_TABLES" "$DIR_TABLES_UNIFORM"
	$RM_RF "$DIR_SETUP" "$DIR_DATA" "$DIR_RESULT" "$DIR_PLOT_SCRIPTS"
}

# $1 - 'client', 'server', 'console'
# stdout: list of space-separated ssh_addr and LNET NID, one per line
ssh_list_type()
{
	local i
	local index

	if [ "$1" == "console" ]; then
		echo $CONSOLE
		return
	fi
	for index in $(seq 0 $(expr $NODES_NR - 1)); do
		local role=$(node_get $index "role")
		local ssh_addr=$(node_get $index "ssh_addr")
		if [ "$role" == "$1" ]; then
			echo "$ssh_addr"
		fi
	done
}

nids_get()
{
	ssh_list_type "console" | $SCRIPT_LIST_NIDS > $FILE_CONSOLE_NID
	ssh_list_type "client"	| $SCRIPT_LIST_NIDS > $FILE_CLIENTS_NID
	ssh_list_type "server"	| $SCRIPT_LIST_NIDS > $FILE_SERVERS_NID
	CONSOLE_NID=$(cat $FILE_CONSOLE_NID | nid_filter)
}

nid_filter()
{
	# awk script: select first NID for each host
	# @todo make only one call to this script
	local NID_SELECT="{ if (!(x[\$1]++) && \$2 ~ /$NET_TYPE\$/) print \$0 }"
	awk "$NID_SELECT" | head -n 1 | awk '{print $2}'
}

node_nid_select()
{
	local index=$1
	local role=$(node_get $index "role")
	local file
	local nid

	case "$role" in
	client)	file=$FILE_CLIENTS_NID ;;
	server)	file=$FILE_SERVERS_NID ;;
	esac
	nid=$(cat $file | nid_filter)
	node_set $index "nid" $nid
}

# $1 - node index
# $2 - see $1 in nodes_list_cmdline()
# $3 - see $2 in nodes_list_cmdline()
node_list_params()
{
	local index=$1
	local space=$3
	local role=$(node_get $index "role")
	if [ "$role" == "$2" ]; then
		local ssh_addr=$(node_get $index "ssh_addr")
		local cmdline=$(node_get $index "cmdline_$space")
		echo "$ssh_addr $space $cmdline"
	fi
}

# $1 - 'client' or 'server'
# $2 - 'cmdline_user' or 'cmdline_kernel'
nodes_list_params()
{
	for_each_node node_list_params $@
}

configure()
{
	local space

	nids_get
	console_configure1
	for_each_node node_nid_select
	for_each_node node_configure
	console_configure2

	for space in "user" "kernel"; do
		nodes_list_params "client" "$space" > "$FILE_CLIENTS_CMD-$space"
		nodes_list_params "server" "$space" > "$FILE_SERVERS_CMD-$space"
	done
	CONSOLE_CMDLINE="$CONSOLE user $CONSOLE_CMDLINE"
	echo $CONSOLE_CMDLINE > "$FILE_CONSOLE_CMD"

	echo "$MSG_SIZE_LIST" > $FILE_MSG_SIZE
	echo "$CONCURRENCY_CLIENT_LIST" > $FILE_CONCURRENCY
}

config_load()
{
	MSG_SIZE_LIST=$(cat $FILE_MSG_SIZE)
	MSG_SIZE_ARR=(${MSG_SIZE_LIST// / })
	CONCURRENCY_CLIENT_LIST=$(cat $FILE_CONCURRENCY)
	CONCURRENCY_CLIENT_ARR=(${CONCURRENCY_CLIENT_LIST// / })
	CONSOLE_CMDLINE=$(cat $FILE_CONSOLE_CMD)
}

# TODO current options are for 1 client/1 server
console_cmdline_complement()
{
	local test_type=$1
	local concurrency=$2
	local msg_size=$3

	CONCURRENCY_CLIENT=$concurrency
	CONCURRENCY_SERVER=$(expr $concurrency "*" 2)
	$ECHO_N "-t $test_type "
	$ECHO_N "-n $MSG_NR "
	$ECHO_N "-T $TEST_RUN_TIME "
	$ECHO_N "-s $msg_size "
	$ECHO_N "-E $CONCURRENCY_SERVER "
	$ECHO_N "-e $CONCURRENCY_CLIENT "
	$ECHO_N "-p "
	if [ "$test_type" == "bulk" ]; then
		BD_BUF_NR_CLIENT=$(expr $concurrency "*" 4)
		BD_BUF_NR_SERVER=$(expr $concurrency "*" 4)
		$ECHO_N "-B $BD_BUF_NR_SERVER "
		$ECHO_N "-b $BD_BUF_NR_CLIENT "
		$ECHO_N "-f $BD_BUF_SIZE "
		$ECHO_N "-g $BD_NR_MAX "
	fi
}

test_params_echo()
{
	local space_clients="$1"
	local space_servers="$2"
	shift 2
	echo "$@"
	cat "$FILE_CLIENTS_CMD-$space_clients"
	cat "$FILE_SERVERS_CMD-$space_servers"
}

dir3()
{
	local test_type=$1
	local space_client=$2
	local space_server=$3

	$ECHO_N "$test_type-$space_client-$space_server"
}

test_file_dir()
{
	local test_type=$1
	local space_client=$2
	local space_server=$3

	$ECHO_N "$DIR_DATA/$(dir3 $@)"
}

test_table_prefix()
{
	local dir_name="$1"
	local parent_dir="${2-$DIR_TABLES}"

	$ECHO_N "$parent_dir/$dir_name"
}

test_table_prefix3()
{
	test_table_prefix $(dir3 $@)
}

table_filename()
{
	local measurement="$1"
	local m_type="$2"
	local table_prefix="$3"

	echo "$table_prefix-$measurement-$m_type"
}

test_file_prefix()
{
	local test_type=$1
	local space_client=$2
	local space_server=$3
	local concurrency=$4
	local msg_size=$5

	$ECHO_N "$(test_file_dir $@)/$concurrency-$msg_size"
}

dirs_make()
{
	$MKDIR "$(test_file_dir $@)"
}

test_run()
{
	local test_type=$1
	local space_client=$2
	local space_server=$3
	local concurrency=$4
	local msg_size=$5
	local file_raw="$(test_file_prefix $@)-raw"
	local console_cmdline

	# skip already finished test
	if [ -f "$file_raw" ]; then
		return
	fi
	console_cmdline="$(console_cmdline_complement $test_type \
			$concurrency $msg_size)"
	console_cmdline="$CONSOLE_CMDLINE $console_cmdline"

	test_params_echo "$space_client" "$space_server" "$console_cmdline" | \
		$SCRIPT_TEST_RUN > $file_raw
}

table_set0()
{
	local dir_name="$1"
	local measurement="$2"
	local m_type="$3"
	local table_prefix="$(test_table_prefix $dir_name)"
	table_0 > "$(table_filename $measurement $m_type $table_prefix)"
}

# $1 - function to call
for_each_combination()
{
	local test_type
	local space_client
	local space_server
	local concurrency
	local msg_size
	local func="$1"
	shift 1
	for test_type in "ping" "bulk"; do
	    for space_client in "user" "kernel"; do
		for space_server in "user" "kernel"; do
		    for concurrency in $CONCURRENCY_CLIENT_LIST; do
			for msg_size in $MSG_SIZE_LIST; do
				"$func" $test_type $space_client $space_server \
					$concurrency $msg_size $@
			done
		    done
		done
	    done
	done
}

table_0()
{
	local concurrency
	local filler0=$(yes 0 | head -n ${#MSG_SIZE_ARR[@]})

	$ECHO_N "header "
	$ECHO ${MSG_SIZE_ARR[@]}
	for concurrency in "${CONCURRENCY_CLIENT_ARR[@]}"; do
		$ECHO_N "$concurrency "
		$ECHO $filler0
	done
}

table_set()
{
	local filename="$1"
	local msg_size="$2"
	local concurrency="$3"
	local value="$4"
	local AWK_SCRIPT="{if (\$1 == $concurrency)
			   { \$${MSG_SIZE_REVERSE[$msg_size]} = \"$value\" };
			   print}"

	awk "$AWK_SCRIPT" "$filename" > "$filename.NEW"
	mv -f "$filename.NEW" "$filename"
}

# Output: value
console_value_get()
{
	local file_raw="$1"
	local role="$2"
	local name="$3"

	tail -n 2 "$file_raw" | grep "^$role" | tr ' ' '\n' | \
		grep -A 1 "$name" | tail -n 1
}

for_each_measurement()
{
	local measurement
	local m_type
	local func=$1
	shift 1
	for measurement in ${MEASUREMENT_ARR[@]}; do
		for m_type in ${M_TYPE_ARR[@]}; do
			"$func" $measurement $m_type $@
		done
	done
}

console_measurement_parse()
{
	local measurement="$1"
	local m_type="$2"
	local table_prefix="$3"
	local file_raw="$4"
	local test_type="$5"
	local msg_size="$6"
	local concurrency="$7"
	local value
	local table_file="$(table_filename $measurement $m_type $table_prefix)"

	case "$measurement" in
	"Bandwidth"|"MPS")
		# total bandwidth (in + out)
		# mps_received_ * msg_size * 2
		value=1
		[ "$measurement" == "Bandwidth" ] && value="$msg_size"
		local mps_received=$(console_value_get "$file_raw" client \
				     mps_received_$m_type)
		value="$(expr $mps_received \* $value \* 2)" ;;
	"RTT")
		# TODO save and draw server RTT for bulk tests
		value="$(console_value_get "$file_raw" client rtt_$m_type)" ;;
	esac

	table_set "$table_file" "$msg_size" "$concurrency" "$value"
}

console_output_parse()
{
	local test_type=$1
	local space_client=$2
	local space_server=$3
	local concurrency=$4
	local msg_size=$5
	local file_raw="$(test_file_prefix $@)-raw"
	local table_prefix="$(test_table_prefix3 $@)"

	for_each_measurement console_measurement_parse "$table_prefix" \
		"$file_raw" "$test_type" "$msg_size" "$concurrency"
}

sample_get()
{
	local msg_size="$1"
	local concurrency="$2"
	local dir_name="$3"
	local measurement="$4"
	local m_type="$5"
	local table_prefix="$(test_table_prefix $dir_name)"
	local table_file="$(table_filename $measurement $m_type $table_prefix)"
	local AWK_SCRIPT="{if (\$1 == $concurrency)
			   print \$${MSG_SIZE_REVERSE[$msg_size]}}";
	awk "$AWK_SCRIPT" "$table_file" | head -n 1
}

iterate_ox_msg_size()
{
	local samples_get="$1"
	shift 1
	local msg_size
	[ "$1" == "0X" ] || exit 2
	$ECHO $($samples_get "HEADER_GET" "$2" "$3" "$4" "$5" "HEADER_GET")
	for msg_size in ${MSG_SIZE_ARR[@]}; do
		$ECHO_N "$msg_size "
		$ECHO $($samples_get "$msg_size" "$2" "$3" "$4" "$5" "EMPTY")
	done
}

iterate_ox_concurrency()
{
	local samples_get="$1"
	local concurrency
	shift 1
	[ "$2" == "0X" ] || exit 2
	$ECHO $($samples_get "$1" "$2" "$3" "$4" "$5" "HEADER_GET")
	for concurrency in ${CONCURRENCY_CLIENT_ARR[@]}; do
		$ECHO_N "$concurrency "
		$ECHO $($samples_get "$1" "$concurrency" "$3" "$4" "$5" "EMPTY")
	done
}

samples_get_msg_size()
{
	local cmd="$6"
	local msg_size

	if [ "$cmd" == "HEADER_GET" ]; then
		$ECHO_N "HEADER "
		$ECHO "${MSG_SIZE_ARR[@]}"
		return
	fi
	[ "$1" == "SAMPLES" ] || exit 2
	for msg_size in ${MSG_SIZE_ARR[@]}; do
		$ECHO_N " "
		$ECHO_N $(sample_get "$msg_size" "$2" "$3" "$4" "$5")
	done
}

samples_get_concurrency()
{
	local cmd="$6"
	local concurrency

	if [ "$cmd" == "HEADER_GET" ]; then
		$ECHO_N "HEADER "
		$ECHO "${CONCURRENCY_CLIENT_ARR[@]}"
		return
	fi
	[ "$2" == "SAMPLES" ] || exit 2
	for concurrency in ${CONCURRENCY_CLIENT_ARR[@]}; do
		$ECHO_N " "
		$ECHO_N $(sample_get "$1" "$concurrency" "$3" "$4" "$5")
	done
}

samples_get_dir_name()
{
	local cmd="$6"
	local dir_name

	if [ "$cmd" == "HEADER_GET" ]; then
		$ECHO_N "HEADER "
		for dir_name in ${DIR_NAME_ARR[@]}; do
			echo "\"$dir_name\" "
		done
		return
	fi
	[ "$3" == "SAMPLES" ] || exit 2
	for dir_name in ${DIR_NAME_ARR[@]}; do
		$ECHO_N " "
		$ECHO_N $(sample_get "$1" "$2" "$dir_name" "$4" "$5")
	done
}

samples_get_m_type()
{
	local cmd="$6"
	local m_type

	if [ "$cmd" == "HEADER_GET" ]; then
		$ECHO_N "HEADER "
		$ECHO "${M_TYPE_ARR[@]}"
		return
	fi
	[ "$5" == "SAMPLES" ] || exit 2
	for m_type in ${M_TYPE_ARR[@]}; do
		$ECHO_N " "
		$ECHO_N $(sample_get "$1" "$2" "$3" "$4" "$m_type")
	done
}

plot_script_2d()
{
	local title="$1"
	local plot_file="$2"
	local out_file="$3"
	local ox="$4"
	local oy="$5"
	local NF=$(head -n 1 "$plot_file" | awk '{print NF}')

	plot_header "$title" "$out_file"
	$ECHO "set grid"
	# $ECHO "set datafile missing \"-\""
	$ECHO "set xtics nomirror rotate by -45 font \",8\""
	$ECHO "set key noenhanced"
	$ECHO "set style data linespoints"
	$ECHO "set logscale y"
	plot_axis_label "x" "$ox"
	plot_axis_label "y" "$oy"
	plot_axis_format "y" "$oy"
	$ECHO "plot '$plot_file' using 2:xtic(1) title columnheader(2),	\\"
	$ECHO "for [i=3:$NF] '' using i title columnheader(i)"
}

draw_graph_2d_prepare()
{
	local func_iterate="$1"
	local func_sample_get="$2"
	local title="$3"
	shift 3
	local msg_size="$1"
	local concurrency="$2"
	local dir_name="$3"
	local measurement="$4"
	local m_type="$5"
	# local root="$msg_size-$concurrency-$dir_name-$measurement-$m_type"
	local root="2d-$measurement-$m_type-$dir_name-$msg_size-$concurrency"
	local script_prefix="$DIR_PLOT_SCRIPTS/$root"
	local plot_file="$script_prefix.txt"
	local script_file="$script_prefix.gnu"
	local out_file="$DIR_RESULT/$root.png"

	iterate_ox_$func_iterate samples_get_$func_sample_get $@ > "$plot_file"
	plot_script_2d "$title" "$plot_file" "$out_file" \
		"$func_iterate" "$measurement" > "$script_file"
}

draw_graph_2d_measurement()
{
	local measurement="$1"
	local title_prefix="$2"
	local msg_size
	local concurrency
	local dir_name
	local m_type
	local title
	local script_prefix

	dir_name="SAMPLES"
	m_type="avg"
	msg_size="0X"
	for concurrency in ${CONCURRENCY_CLIENT_ARR[@]}; do
		title="$title_prefix. Concurrency = $concurrency"
		draw_graph_2d_prepare msg_size dir_name \
			"$title" "$msg_size" "$concurrency" \
			"$dir_name" "$measurement" "$m_type"
	done
	concurrency="0X"
	for msg_size in ${MSG_SIZE_ARR[@]}; do
		title="$title_prefix. Test message size = $msg_size bytes"
		draw_graph_2d_prepare concurrency dir_name \
			"$title" "$msg_size" "$concurrency" \
			"$dir_name" "$measurement" "$m_type"
	done
	msg_size="0X"
	concurrency="SAMPLES"
	for dir_name in ${DIR_NAME_ARR[@]}; do
		title="$title_prefix. $dir_name"
		draw_graph_2d_prepare msg_size concurrency \
			"$title" "$msg_size" "$concurrency" \
			"$dir_name" "$measurement" "$m_type"
	done
	msg_size="SAMPLES"
	concurrency="0X"
	for dir_name in ${DIR_NAME_ARR[@]}; do
		title="$title_prefix. $dir_name"
		draw_graph_2d_prepare concurrency msg_size \
			"$title" "$msg_size" "$concurrency" \
			"$dir_name" "$measurement" "$m_type"
	done
	msg_size="0X"
	m_type="SAMPLES"
	for concurrency in ${CONCURRENCY_CLIENT_ARR[@]}; do
		for dir_name in ${DIR_NAME_ARR[@]}; do
			title="$title_prefix. Concurrency = $concurrency"
			title="$title, $dir_name"
			draw_graph_2d_prepare msg_size m_type \
				"$title" "$msg_size" "$concurrency" \
				"$dir_name" "$measurement" "$m_type"
		done
	done
	concurrency="0X"
	for msg_size in ${MSG_SIZE_ARR[@]}; do
		for dir_name in ${DIR_NAME_ARR[@]}; do
			title="$title_prefix. Test message size = $msg_size"
			title="$title, $dir_name"
			draw_graph_2d_prepare concurrency m_type \
				"$title" "$msg_size" "$concurrency" \
				"$dir_name" "$measurement" "$m_type"
		done
	done
}

draw_graphs_2d()
{
	declare -A title_start

	title_start["Bandwidth"]="Bandwidth, bytes/s"
	title_start["RTT"]="Round-trip time, ns"
	title_start["MPS"]="Messages per Second"
	for measurement in ${MEASUREMENT_ARR[@]}; do
		draw_graph_2d_measurement "$measurement" \
			"${title_start[$measurement]}"
	done
}

table_make_uniform()
{
	sed 1d | awk '{$1=""; print}' | sed -e 's/^[ \t]*//'
}

# make gnuplot uniform tables (without axis ticks)
table_additional_generate()
{
	local dir_name="$1"
	local measurement="$2"
	local m_type="$3"
	local table="$(table_filename $measurement $m_type \
		       $(test_table_prefix $dir_name))"
	local table_uniform="$(table_filename $measurement $m_type \
			$(test_table_prefix $dir_name $DIR_TABLES_UNIFORM))"

	table_make_uniform < "$table" > "$table_uniform"
}

plot_header()
{
	local title="$1"
	local img_file="$2"

	$ECHO "reset"
	$ECHO "set terminal png"
	$ECHO "set out \"$img_file\""
	$ECHO "set title \"$title\""
	$ECHO "set timestamp"
}

plot_3d_header()
{
	local title="$1"
	local img_file="$2"

	plot_header "$@"
	$ECHO "set logscale z"
	$ECHO "set contour both"
	$ECHO "set grid"
}

plot_3d_table()
{
	local dir_name="$1"
	local measurement="$2"
	local m_type="$3"
	local table_file="$(table_filename $measurement $m_type \
			    $(test_table_prefix $dir_name $DIR_TABLES_UNIFORM))"
	$ECHO_N "splot \"$table_file\" matrix "
	[ "$measurement" == "RTT" ] && $ECHO_N "using 1:2:(\$3/1E+9) "
	$ECHO "with lines title \"$measurement-$m_type\""
}

plot_ticks_bcount_IEC()
{
	local size="$1"

	if [ $size -lt 1024 ]; then
		$ECHO_N "${size}B"
	elif [ $size -lt $(expr 1024 "*" 1024) ]; then
		$ECHO_N "$(expr $size / 1024)KiB"
	else
		$ECHO_N "$(expr $size / 1024 / 1024)MiB"
	fi
}

# $1 - "x" or "y"
# text only for even ticks (0, 2, ...)
plot_ticks_msg_size()
{
	local ticks=""
	local skip="$1"
	local i=0
	local msg_size

	for msg_size in ${MSG_SIZE_ARR[@]}; do
		[ $i -ne 0 ] && ticks="$ticks, "
		if [ $(expr $i % $skip) -eq 0 ]; then
			ticks="$ticks\"$(plot_ticks_bcount_IEC $msg_size)\" $i"
		else
			ticks="$ticks\"\" $i"
		fi
		((i++)) || true
	done
	$ECHO_N "$ticks"
}

plot_ticks_concurrency()
{
	local ticks=""
	local skip="$1"
	local i=0
	local concurrency

	for concurrency in ${CONCURRENCY_CLIENT_ARR[@]}; do
		[ $i -ne 0 ] && ticks="$ticks, "
		if [ $(expr $i % $skip) -eq 0 ]; then
			ticks="$ticks\"$concurrency\" $i"
		else
			ticks="$ticks\"\" $i"
		fi
		((i++)) || true
	done
	$ECHO_N "$ticks"
}

plot_ticks()
{
	local axis="$1"
	local param="$2"
	local skip="$3"
	$ECHO "set ${axis}tics font \",9\" ($(plot_ticks_${param} $skip))"
}

plot_axis_label()
{
	local axis="$1"
	local measurement="$2"
	declare -A measurement_axis_label

	measurement_axis_label["msg_size"]="Test message size"
	measurement_axis_label["concurrency"]="Concurrency"
	measurement_axis_label["Bandwidth"]="Bandwidth"
	measurement_axis_label["RTT"]="Round-trip time"
	measurement_axis_label["MPS"]="Messages Per Second"
	$ECHO "set ${axis}label \"${measurement_axis_label[$measurement]}\""
}

plot_cntrparam()
{
	local measurement="$1"
	local bandwidth=""
	local bandwidth1
	local percent
	declare -A measurement_param

	for percent in 10 50 90 98; do
		bandwidth1=$(echo "scale=3; \
			     $percent / 100 * $NET_BANDWIDTH_MAX" | bc)
		bandwidth="$bandwidth,$bandwidth1"
	done
	measurement_param["Bandwidth"]="$(echo $bandwidth | sed s/^.//)"
	measurement_param["RTT"]="1E-4,3E-4,7E-4,1E-3,5E-3"
	measurement_param["MPS"]="1E+3,3E+3,1E+4,3E+4,1E+5"
	$ECHO "set cntrparam levels discrete ${measurement_param[$measurement]}"
}

declare -A measurement_format
measurement_format["Bandwidth"]="%.2b %BB/s"
measurement_format["msg_size"]="%.2b %BB"
measurement_format["concurrency"]="%.0f"
measurement_format["MPS"]="%.0f"
measurement_format["RTT"]="%.0s %cs"

plot_clabel()
{
	local measurement="$1"

	$ECHO "set clabel \"${measurement_format[$measurement]}\""
}

plot_axis_format()
{
	local axis="$1"
	local measurement="$2"
	$ECHO "set format $axis \"${measurement_format[$measurement]}\""
	if [ "$axis" == "z" ]; then
		$ECHO "set ztics font \",8\""
	fi
}

plot_axis()
{
	local axis="$1"
	local measurement="$2"
	local ticks_skip="$3"

	plot_ticks "$axis" "$measurement" "$ticks_skip"
	plot_axis_label "$axis" "$measurement"
	plot_axis_format "$axis" "$measurement"
}

plot_pm3d()
{
	local measurement="$1"

	$ECHO "set pm3d at bstbst map"
	$ECHO_N "set palette"
	[ "$measurement" == "RTT" ] && $ECHO_N " negative"
	$ECHO
	$ECHO "set cbtics format " \
	      "\"${measurement_format[$measurement]}\" font \",8\""
	[ "$measurement" == "RTT" ] && $ECHO "set logscale cb"
}

plot_3d_fence()
{
	true
}

draw_graph_3d_table()
{
	local dir_name="$1"
	local measurement="$2"
	local m_type="$3"
	local draw_type="$4"
	local file_root="$draw_type-$measurement-$dir_name-$m_type"
	local plot_file="$DIR_PLOT_SCRIPTS/$file_root.gnu"
	local img_file="$DIR_RESULT/$file_root.png"
	local title="$file_root"

	plot_3d_header "$title" "$img_file" > "$plot_file"
	plot_axis "x" "msg_size" 2 >> "$plot_file"
	plot_axis "y" "concurrency" 3 >> "$plot_file"
	# plot_axis_label "z" "$measurement" >> "$plot_file"
	plot_axis_format "z" "$measurement" >> "$plot_file"
	plot_cntrparam "$measurement" >> "$plot_file"
	plot_clabel "$measurement" >> "$plot_file"
	[ "$draw_type" == "pm3d" ] && \
		plot_pm3d "$measurement" >> "$plot_file"
	plot_3d_table "$@" >> "$plot_file"
}

draw_graph_3d_measurement()
{
	local measurement="$1"
	local title_prefix="$2"
	local dir_name
	local m_type
	local plot_file
	local title

	for_each_table draw_graph_3d_table "3d"
	for_each_table draw_graph_3d_table "pm3d"
	# TODO fence graphs (if needed)
	return
	for dir_name in ${DIR_NAME_ARR[@]}; do
		plot_file="$DIR_PLOT_SCRIPTS/3d-$measurement-$dir_name.gnu"
		img_file="$DIR_RESULT/3d-$measurement-$dir_name.png"
		title="$title_prefix. $dir_name"
		for m_type in ${M_TYPE_ARR[@]}; do
			plot_3d_table "$dir_name" "$measurement" \
				"$m_type" >> "$plot_file"
		done
	done
}

for_each_table()
{
	local func="$1"
	shift 1
	for dir_name in ${DIR_NAME_ARR[@]}; do
		for measurement in ${MEASUREMENT_ARR[@]}; do
			for m_type in ${M_TYPE_ARR[@]}; do
				$func "$dir_name" "$measurement" "$m_type" "$@"
			done
		done
	done
}

draw_graphs_3d()
{
	declare -A title_start

	for_each_table table_additional_generate
	title_start["Bandwidth"]="Bandwidth"
	title_start["RTT"]="Round-trip time"
	title_start["MPS"]="Messages per Second"
	for measurement in ${MEASUREMENT_ARR[@]}; do
		draw_graph_3d_measurement "$measurement" \
			"${title_start[$measurement]}"
	done
}

gnuplot_run()
{
	find "$DIR_PLOT_SCRIPTS" -name "*.gnu" | xargs -L1 $GNUPLOT
}

draw_graphs()
{
	draw_graphs_2d
	draw_graphs_3d
	gnuplot_run
}

main "$@"
