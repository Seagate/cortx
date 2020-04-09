#!/usr/bin/env bash

## Declare all variables
CL_DIR_TAG=""
CL_DIR_PATH=""
M0VG=""
CL_HOME="$HOME/virtual_clusters"
FORCE="FALSE"

destroy_cluster()
{
    local FSTART=$(date +%s); local RESULT=0
    echo -e "\nHere to destroy $CL_DIR_TAG in the $CL_DIR_PATH"
    $M0VG status
    if [ "$FORCE" == "FALSE" ]; then
        echo -en "\nYou are about to destroy virtual cluster $CL_DIR_TAG at $CL_DIR_PATH. Type [YES] in 30 seconds to continue. "
        read -t 30 CH
        if [ "$CH" != "YES" ]; then
            echo -e "\nVirtual cluster $CL_DIR_TAG at $CL_DIR_PATH was not destroyed."
            echo -e "\n\n"
            return $RESULT
        fi
        echo -e "\nThanks for confirmation"
    fi
    $M0VG destroy --force
    RESULT=$?
    rm -rf $CL_DIR_PATH
    FTIME=$(( $(date +%s) - $FSTART ))
    echo -e "\nCluster $CL_DIR_TAG destroyed in $FTIME seconds."
}

print_usage()
{
        echo -e "\nInvalid Args."
        echo -e "\n./destroy-cluster.sh <CLUSTER-NAME> [--force]"
        echo -e "\n\n"
}

### main()
if [ $# == 2 ]; then
    if [ "$2" == "--force" ]; then
        FORCE="TRUE"
    else
        print_usage
        exit 1
    fi
    CL_DIR_TAG=$1
elif [ $# == 1 ]; then
    CL_DIR_TAG=$1
else
    print_usage
    exit 1
fi

CL_DIR_PATH=$CL_HOME/$CL_DIR_TAG
M0VG=$CL_DIR_PATH/mero/scripts/m0vg

echo -e "\n[$FUNCNAME: $LINENO] CL_DIR_TAG [$CL_DIR_TAG]"
echo -e "\n[$FUNCNAME: $LINENO] CL_DIR_PATH [$CL_DIR_PATH]"
echo -e "\n[$FUNCNAME: $LINENO] M0VG [$M0VG]"

if [ ! -d "$CL_DIR_PATH" ]; then
    echo -e "\n[$FUNCNAME: $LINENO] Cluster Path $CL_DIR_PATH doesnot exist!!"
    exit 0
fi

destroy_cluster
exit 0
