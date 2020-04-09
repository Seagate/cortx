#!/bin/bash

# A simple wrapper for Clovis commands

# Get the location of this script and look for the command in the
# same directory
dir=`dirname $0`

#. $dir/mero_config.sh

# Get local address
modprobe lnet
lctl network up &>> /dev/null
LOCAL_NID=`lctl list_nids | head -1`
LOCAL_EP=$LOCAL_NID:12345:33:100
HA_EP=$LOCAL_NID:12345:34:1
PROF_OPT='<0x7000000000000001:0>'
PROC_FID='<0x7200000000000000:0>'
INDEX_DIR="/tmp"
cmd_args="$LOCAL_EP $HA_EP $CONFD_EP '$PROF_OPT' '$PROC_FID' $INDEX_DIR"

echo -n "Enter ID of object to operate on: "
read KEY
cmd_args="$cmd_args $KEY"

if [ "$1" = "c0cat" ]; then
	echo -n "Enter the blocksize of the object: "
	read BLOCKSIZE

	echo -n "Enter the number of blocks to read: "
	read BLOCKCOUNT

	cmd_args="$cmd_args $BLOCKSIZE $BLOCKCOUNT"

	echo -n "Enter the name of the file to output to: "
	read OUTPUT

	cmd_args="$cmd_args > $OUTPUT"
fi;

if [ "$1" = "c0cp" ] || [ "$1" = "c0cp_mt" ]; then
	echo -n "Enter the name of the file to copy from: "
	read SRC_FILE

	echo -n "Enter the blocksize of the object: "
	read BLOCKSIZE

	echo -n "Enter the number of blocks to copy: "
	read BLOCKCOUNT

	cmd_args="$cmd_args $SRC_FILE $BLOCKSIZE $BLOCKCOUNT"
fi;

# Assembly command
cmd_exec="$dir/$1"
cmd="$cmd_exec $cmd_args"

# Run it
echo "# $cmd" >/dev/tty

eval $cmd || {
	echo "Failed to run command $1"
}

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
