#!/usr/bin/env bash

## This is our rescue script when there is system overload because of 
## un-cleaned VMs lying in the system.

SCRIPT_DIR="`dirname $0`"
DIR_TO_CLEAN="$HOME/virtual_clusters"

if [ "$1" != "" ]; then
	DIR_TO_CLEAN="$1"
fi

if [ ! -d $DIR_TO_CLEAN ]; then
	echo "Path $DIR_TO_CLEAN doesnot exist." 
	exit 1
fi

echo ""
echo ""
echo "THIS IS A SUPER CLEANUP SCRIPT !!!!"
echo "ALL VIRTUAL CLUSTERS & FILES CREATED UNDER [$DIR_TO_CLEAN] WILL BE DESTROYED & DELETED."
echo ""
echo "USE WITH EXTREME CAUTION !! !! !!"
echo ""
echo ""
echo "Executing from $SCRIPT_DIR"
echo ""
echo ""
echo "Following VMs are existing in your user account"
vagrant global-status --prune
echo ""
echo ""
echo "Following files exist in the [$DIR_TO_CLEAN] path."
echo ""
echo ""
ls -l $DIR_TO_CLEAN
echo -n "Do you want to clean-up every file/cluster? Type [YES] in 30 seconds to continue. "
read -t 30 CH 
if [ "$CH" != "YES" ]; then
	echo "No changes were done"
	echo ""
	echo ""
	exit 0
fi

echo "No looking back now !!!"

for CLSTR_PATH in $DIR_TO_CLEAN/*; do
    if [ -d "${CLSTR_PATH}" ]; then
		CLSTR="`basename $CLSTR_PATH`"
		$SCRIPT_DIR/destroy-cluster.sh $CLSTR --force
    fi
done

echo ""
echo ""
echo "Done !!!"
exit 0

