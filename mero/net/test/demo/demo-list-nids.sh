#!/usr/bin/env bash
# set -eux

SSH="ssh"

main()
{
	while read addr; do
		ssh_get_nids $addr | awk "{print \"$addr \" \$0}"
	done
}

ssh_sudo()
{
	addr=$1
	shift 1

	$SSH "$addr" sudo "$@"
}

ssh_get_nids()
{
	addr=$1

	ssh_sudo "$1" modprobe lnet
	ssh_sudo "$1" lctl network up > /dev/null
	ssh_sudo "$1" lctl list_nids
}

main "$@"
