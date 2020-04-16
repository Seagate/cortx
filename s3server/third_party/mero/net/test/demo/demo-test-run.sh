#!/usr/bin/env bash
# set -eux

# Set this to 1 to enable removing of m0.trace.* files
TRACE_RM=0

declare -A NODES
NODES_NR=0

CONSOLE_SSH=
CONSOLE_CMD=

SSH="ssh"

INIT_TIME_S=3
STOP_TIME_S=1

DIR_SCRIPT=${0%/*}
TOP_SCRDIR="$DIR_SCRIPT/../../.."
# TODO hardcoded for now
MOD_M0GF="$TOP_SCRDIR/extra-libs/gf-complete/src/linux_kernel/m0gf.ko"
MOD_M0MERO="$TOP_SCRDIR/m0mero.ko"
PARAMS_M0MERO="node_uuid=00000000-0000-0000-0000-000000000000"

main()
{
	local line
	local node_ssh
	local node_space
	local node_cmd

	read line
	CONSOLE_SSH=$(echo "$line" | awk "{print \$1}")
	CONSOLE_CMD="$(echo $line | cmd_get)"
	while read line; do
		node_ssh=$(echo "$line" | awk "{print \$1}")
		node_space=$(echo "$line" | awk "{print \$2}")
		node_cmd="$(echo $line | cmd_get)"
		node_set $NODES_NR "ssh" "$node_ssh"
		node_set $NODES_NR "space" "$node_space"
		node_set $NODES_NR "cmd" "$node_cmd"
		((NODES_NR++)) || true
	done
	host_pre "$CONSOLE_SSH"
	for_each_node node_host_pre
	for_each_node node_pre
	sleep $INIT_TIME_S
	console_run
	sleep $STOP_TIME_S
	for_each_node node_post
	for_each_node node_host_post
	host_post "$CONSOLE_SSH"
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

ssh_sudo()
{
	local ssh_credentials="$1"
	shift 1
	$SSH "$ssh_credentials" sudo "$@"
}

cmd_get()
{
	awk "{\$1 = \"\"; \$2 = \"\"; print}" | sed -e 's/^[ \t]*//'
}

console_run()
{
	ssh_sudo "$CONSOLE_SSH" "$CONSOLE_CMD"
	[ $TRACE_RM -eq 1 ] && ssh_sudo "$CONSOLE_SSH" "rm -rf m0.trace.*"
}

host_pre()
{
	local ssh_credentials="$1"
	ssh_sudo "$ssh_credentials" modprobe lnet
	ssh_sudo "$ssh_credentials" lctl network up
	ssh_sudo "$ssh_credentials" insmod "$MOD_M0GF"
	ssh_sudo "$ssh_credentials" insmod "$MOD_M0MERO" "$PARAMS_M0MERO"
	[ $TRACE_RM -eq 1 ] && ssh_sudo "$ssh_credentials" "rm -rf m0.trace.*"
}

host_post()
{
	local ssh_credentials="$1"
	ssh_sudo "$ssh_credentials" rmmod m0mero
	ssh_sudo "$ssh_credentials" rmmod m0gf
}

node_host_pre()
{
	host_pre "$(node_get $1 ssh)"
}

node_host_post()
{
	host_post "$(node_get $1 ssh)"
}

node_kernel_pre()
{
	local ssh_credentials="$(node_get $1 ssh)"
	local module="$(node_get $1 cmd)"
	ssh_sudo "$ssh_credentials" insmod "$module"
}

node_kernel_post()
{
	local ssh_credentials="$(node_get $1 ssh)"
	ssh_sudo "$ssh_credentials" rmmod m0nettestd
}

node_user_pre()
{
	local ssh_credentials="$(node_get $1 ssh)"
	local cmd="$(node_get $1 cmd)"
	ssh_sudo "$ssh_credentials" "$cmd" &
}

node_user_post()
{
	true
}

node_pre()
{
	node_$(node_get $1 space)_pre "$@"
}

node_post()
{
	node_$(node_get $1 space)_post "$@"
}

main "$@"
