#!/usr/bin/env bash

MODLIST="m0mero.ko"

abort()
{
    msg="$1"
    echo "$1 Aborting."
    exit 1
}

modload()
{
    for m in $MODLIST ;do
	insmod $m                   || abort "Error loading $m."
    done
}

modunload()
{
    local rc=0
    for m in $MODLIST ;do
	echo $m
    done | tac | while read ;do
	rmmod $REPLY                || {
		rc=$?
		echo "Error unloading $m."
	}
    done
    return $rc
}

