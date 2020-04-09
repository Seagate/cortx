#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

## Declare all variables
SCRIPT_START=0
HOME_DIR=""
MERO_REPO="http://gerrit.mero.colo.seagate.com/mero"
HARE_REPO="http://gitlab.mero.colo.seagate.com/mero/hare.git"
GIT_LAB="http://gitlab.mero.colo.seagate.com"
CL_DIR_TAG=""
MERO_REF=""
HARE_USER=""
HARE_USER_BRANCH=""
CL_DIR_PATH=""
M0VG=""
CL_HOME="$HOME/virtual_clusters"
ALL_FOUR_VMS_CREATED=""
DESTROY_CLUSTER_SCRIPT=""
VMHOST_PATH_IN_VM="/data/mero/scripts/provisioning/vmhost"
COMPILE_INSTALL_MERO_SCRIPT="$VMHOST_PATH_IN_VM/gci-compile-install-mero.sh"
COMPILE_INSTALL_HARE_SCRIPT="$VMHOST_PATH_IN_VM/gci-compile-install-hare.sh"
START_CLUSTER_SCRIPT="$VMHOST_PATH_IN_VM/gci-start-cluster.sh"
RUN_TESTS_SCRIPT="$VMHOST_PATH_IN_VM/gci-run-tests.sh"

print_usage()
{
    echo -e "\n"
    echo -e "Using HARE_REPO [$HARE_REPO]"
    echo -e "Using MERO_REPO [$MERO_REPO]"
    echo -e "Using M0_VM_BOX_URL [$M0_VM_BOX_URL]"
    echo -e "\n"
    echo -e "./gci-create-cluster.sh <CLUSTER_NAME> <MERO_REF> <HARE_USER> "  \
        "<HARE_USER_BRANCH>"
    echo -e "\t On master HEAD                                        "       \
        "./gci-create-cluster.sh CL0001 \"\" \"\" \"\""
    echo -e "\t On master with MERO_REF                               "       \
        "./gci-create-cluster.sh CL0001 \"refs/changes/28/17128/1\" \"\""
    echo -e "\t On master with HARE_USER and HARE_USER_BRANCH         "       \
        "./gci-create-cluster.sh CL0001 \"\" \"sourish.banerje\" "            \
        "\"alpha_pct\""
    echo -e "\t On master with MERO_REF & HARE_USER, HARE_USER_BRANCH "       \
        "./gci-create-cluster.sh CL0001 \"refs/changes/28/17128/1\" "         \
        "\"sourish.banerjee\" \"alpha_pct\""
    echo -e "\nCLUSTER_NAME can be alpha-numeric only. No special "           \
        "characters allowed."
    echo -e "All params is mandatory, can be empty quotes as in "             \
        "\"\" though."
    echo -e "\n"
}

print_msg()
{
    echo -e "\n[${FUNCNAME[1]}] $1"
}

script_terminate()
{
    local RESULT=$1
    pwd
    cd $HOME_DIR
    print_msg "Using [$DESTROY_CLUSTER_SCRIPT $CL_DIR_TAG]"
    $DESTROY_CLUSTER_SCRIPT $CL_DIR_TAG "--force"
    SCRIPT_TIME=$(( $(date +%s) - $SCRIPT_START ))
    if [ $RESULT != 0 ]; then
        print_msg "Returing $RESULT; Status: FAILED; in $SCRIPT_TIME seconds."
    else
        print_msg "Returing $RESULT; Status: SUCCESS in $SCRIPT_TIME seconds."
    fi
    exit $RESULT
}


print_status_and_time()
{
    local FNAME=${FUNCNAME[1]}
    local FSTART=$1
    local RESULT=$2
    FTIME=$(( $(date +%s) - $FSTART ))
    if [ $RESULT = 0 ]; then
        print_msg "Completed function $FNAME in $FTIME seconds successfully."
    else
        print_msg "Completed function $FNAME in $FTIME seconds with errors."
    fi
}

execute_cmdline()
{
    local CMD=$1
    print_msg "Executing command [$CMD]"
    `$CMD`
    local RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR executing command [$CMD]; Returned [$RESULT]!!!"
    else
        print_msg "Command [$CMD] executed successfully."
    fi
    return $RESULT

}

cleanup_if_existing()
{
    local FSTART=$(date +%s); local RESULT=0
    if [ -d "$CL_DIR_PATH" ]; then
        print_msg "Path [$CL_DIR_PATH] exists!! Cleaning up first !!"
        $M0VG destroy --force
        execute_cmdline "rm -rf $CL_DIR_PATH"
    fi
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

clone_mero()
{
    local FSTART=$(date +%s); local RESULT=0

    print_msg "Executing [git clone --recursive $MERO_REPO]"
    git clone --recursive $MERO_REPO
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR checking out $MERO_REPO!!!"
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    # print_msg "EXECUTING FROM SOURISH's PRIVATE BRANCH !!!"; sleep 5
    # print_msg "Executing [cd mero]"
    # print_msg "Executing [git checkout dev/SB/PREP-SCRIPT-HARE; cd -;]"
    # cd mero
    # git checkout dev/SB/PREP-SCRIPT-HARE; cd -;

    if [ "$MERO_REF" == "" ]; then
        print_msg "MERO_REF was not specified. Compiling  HEAD of Mero " \
            "master branch."
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Pulling MERO_REF [$MERO_REF]"
    cd mero
    print_msg "Executing [git pull $MERO_REPO $MERO_REF]"
    git pull $MERO_REPO $MERO_REF
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR pulling $MERO_REF!!!"
    fi
    cd -
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

use_backup_hare_targz()
{
    local RESULT=0
    print_msg "EXTRACTING HARE from  /home/backup/hare.tar.gz !!!";
    if [ ! -f "/home/backup/hare.tar.gz" ]; then
        print_msg "ERROR file [/home/backup/hare.tar.gz] not found!!!"
        script_terminate 1
    fi

    rm -rf hare
    cp -v /home/backup/hare.tar.gz .
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR [cp -v /home/backup/hare.tar.gz .] failed!!!"
        script_terminate $RESULT
    fi

    tar -zxvf ./hare.tar.gz
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR [tar -zxvf ./hare.tar.gz] failed!!!"
        script_terminate $RESULT
    fi

    rm -v ./hare.tar.gz
    cd hare; git pull; git pull --recurse-submodules; cd -
    # print_msg "WAITING 10 seconds for manual output inspection."
    # read -t 10 a
}

clone_hare()
{
    local FSTART=$(date +%s); local RESULT=0

    print_msg "Executing [git clone --recursive $HARE_REPO]"
    git clone --recursive $HARE_REPO
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "ERROR checking out $HARE_REPO!!!";
        # print_status_and_time  $FSTART $RESULT
        # return $RESULT
        use_backup_hare_targz
    fi

    if [ "$HARE_USER" == "" ] && [ "$HARE_USER_BRANCH" == "" ]; then
        print_msg "HARE_USER & HARE_USER_BRANCH was not specified."
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Pulling HARE_USER [$HARE_USER]; " \
        "HARE_USER_BRANCH [$HARE_USER_BRANCH]"
    cd hare
    print_msg "Executing [git fetch $GIT_LAB/$HARE_USER/hare.git " \
        "$HARE_USER_BRANCH]"
    git fetch "$GIT_LAB/$HARE_USER/hare.git" "$HARE_USER_BRANCH"
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Executing [git checkout -b " \
        "$HARE_USER/hare-$HARE_USER_BRANCH FETCH_HEAD]"
    git checkout -b $HARE_USER/hare-$HARE_USER_BRANCH FETCH_HEAD
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Executing [git fetch origin]"
    git fetch origin
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Executing [git checkout origin/master]"
    git checkout origin/master
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    print_msg "Executing [git merge --no-ff $USER/hare-$HARE_USER_BRANCH]"
    git merge --no-ff $USER/hare-$HARE_USER_BRANCH
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi
    cd -
    return $RESULT
}

use_centos76()
{
    $M0VG env add M0_VM_BOX=centos76/dev
    $M0VG env add M0_VM_BOX_URL="http://ci-storage.mero.colo.seagate.com/vagrant/centos76/dev"
}

use_centos77()
{
    $M0VG env add M0_VM_BOX=centos77/dev
    $M0VG env add M0_VM_BOX_URL="http://ci-storage.mero.colo.seagate.com/vagrant/centos77/dev"
}

edit_m0vg_params()
{
    local FSTART=$(date +%s); local RESULT=0
    print_msg "Editing the m0vg params."
    $M0VG env add M0_VM_HOSTNAME_PREFIX=$CL_DIR_TAG
    $M0VG env add M0_VM_NAME_PREFIX=$CL_DIR_TAG
    $M0VG env add M0_VM_NFS_VERSION=3
    $M0VG env add M0_VM_CMU_MEM_MB=8384
    $M0VG env add M0_VM_CLIENT_NR=1
    $M0VG env add M0_VM_CLIENT_MEM_MB=2046
    $M0VG env add M0_VM_SSU_DISKS=6
    $M0VG env add M0_VM_SSU_DISK_SIZE_GB=2
    # use_centos76
    use_centos77
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

create_vms()
{
    local FSTART=$(date +%s); local RESULT=0

    $M0VG status

    print_msg " Press 0 in 30 seconds to create only the cmu. " \
        "Else all 4 vms will be created."
    read -t 30 CH
    if [ "$CH" == "0" ]; then
        print_msg "Creating only cmu !!!"
        $M0VG up cmu
        RESULT=$?
        if [ $RESULT != 0 ]; then
            print_msg "IGNORED ERROR in creating cmu vm!!!"
            RESULT=0
        fi
        ALL_FOUR_VMS_CREATED="NO"
    else
        print_msg "Creating all 4 VMs (cmu, ssu1, ssu2, client1) !!!"
        $M0VG up cmu ssu1 ssu2 client1
        RESULT=$?
        if [ $RESULT != 0 ]; then
            print_msg "IGNORED ERROR in creating cmu vm!!!"
            RESULT=0
        fi
        ALL_FOUR_VMS_CREATED="YES"
    fi
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

create_cluster()
{
    local FSTART=$(date +%s); local RESULT=0
    print_msg "Creating the cluster [$CL_DIR_TAG]!!!"
    print_msg "Using M0_VM_BOX_URL [$M0_VM_BOX_URL]"
    print_msg "Using MERO_REPO [$MERO_REPO]; MERO_REF [$MERO_REF];"
    print_msg "Using HARE_REPO [$HARE_REPO]; HARE_USER [$HARE_USER]; " \
        "HARE_USER_BRANCH [$HARE_USER_BRANCH];"
    print_msg "Using CL_DIR_PATH [$CL_DIR_PATH];"

    cleanup_if_existing
    mkdir $CL_DIR_PATH; cd $CL_DIR_PATH

    clone_mero
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "Function clone_mero(...) failed !!!"
        print_status_and_time  $FSTART $RESULT
        return $RESULT
    fi

    clone_hare
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "Function clone_hare(...) failed !!!"
        print_status_and_time  $FSTART $RESULT
    fi

    edit_m0vg_params
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "Function edit_m0vg_params(...) failed !!!"
        print_status_and_time  $FSTART $RESULT
    fi

    create_vms
    RESULT=$?
    if [ $RESULT != 0 ]; then
        print_msg "Function create_vms(...) failed !!!"
        print_status_and_time  $FSTART $RESULT
    fi

    cd ..
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

verify_mount()
{
    VM="$1"; local FSTART=$(date +%s); local RESULT=0
    TEST_FILE_VM_PATH="/data/TEST_FILE"
    TEST_FILE_HOST_PATH="$CL_DIR_PATH/TEST_FILE"

    $M0VG run --vm $VM "touch $TEST_FILE_VM_PATH"
    RESULT=$?
    if [ $? = 0 ] && [ -f "$TEST_FILE_HOST_PATH" ]; then
        print_msg "Mount of /data is verified successfully for [$VM];"
        rm -v "$TEST_FILE_HOST_PATH"
    else
        print_msg "Mount of [/data] has failed for $VM."
        print_msg "Press Enter to try reload now, Ctrl+C to terminate. " \
            "Will retry after 30 sec."
        read -t 30 a
        ## CAREFUL -- We are making a recursive call here
        $M0VG reload $VM
        verify_mount $VM
    fi
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}

verify_mount_vms()
{
    local FSTART=$(date +%s); local RESULT=0
    if [ "$ALL_FOUR_VMS_CREATED" == "YES" ]; then
        verify_mount cmu
        verify_mount ssu1
        verify_mount ssu2
        verify_mount client1
    elif [ "$ALL_FOUR_VMS_CREATED" == "NO" ]; then
        verify_mount cmu
    else
        print_msg "Some thing has gone worng with creation of VMs."
        script_terminate -1
    fi
    print_status_and_time  $FSTART $RESULT
    return $RESULT
}


compile_install_mero()
{
    local FSTART=$(date +%s); local RESULT=0
    ## COMPILATION OF MERO
    M0C_START=$(date +%s)
    print_msg " COMPILATION OF MERO STARTED!!! M0C_START [$M0C_START]!!!"
    $M0VG run --vm cmu $COMPILE_INSTALL_MERO_SCRIPT
    RESULT=$?;
    FTIME=$(( $(date +%s) - $FSTART ))
    if [ $RESULT = 0 ]; then
        print_msg "Completed $FUNCNAME in $FTIME successfully."
    else
        print_msg "Completed $FUNCNAME in $FTIME with errors."
    fi
    return $RESULT
}

compile_install_hare()
{
    local FSTART=$(date +%s); local RESULT=0
    ## COMPILATION OF HARE
    H0C_START=$(date +%s)
    print_msg "COMPILATION OF HARE STARTED!!! H0C_START [$H0C_START]!!!"
    $M0VG run --vm cmu $COMPILE_INSTALL_HARE_SCRIPT
    RESULT=$?;
    FTIME=$(( $(date +%s) - $FSTART ))
    if [ $RESULT = 0 ]; then
        print_msg "Completed $FUNCNAME in $FTIME successfully."
    else
        print_msg "Completed $FUNCNAME in $FTIME with errors."
    fi
    return $RESULT
}

start_cluster()
{
    local FSTART=$(date +%s); local RESULT=0
    ## Starting the cluster
    STCL_START=$(date +%s)
    print_msg "Starting the cluster !!!"
    $M0VG run --vm cmu $START_CLUSTER_SCRIPT
    RESULT=$?;
    FTIME=$(( $(date +%s) - $FSTART ))
    if [ $RESULT = 0 ]; then
        print_msg "Completed $FUNCNAME in $FTIME successfully."
    else
        print_msg "Completed $FUNCNAME in $FTIME with errors."
    fi
    return $RESULT
}

run_tests()
{
    local FSTART=$(date +%s); local RESULT=0
    ## Run tests
    print_msg "TESTS WILL BE EXEUTED FROM THE [$RUN_TESTS_SCRIPT]"
    print_msg "To add your own tests, append these to this file."
    $M0VG run --vm cmu $RUN_TESTS_SCRIPT
    RESULT=$?;
    FTIME=$(( $(date +%s) - $FSTART ))
    if [ $RESULT = 0 ]; then
        print_msg "Completed $FUNCNAME in $FTIME successfully."
    else
        print_msg "Completed $FUNCNAME in $FTIME with errors."
    fi
    return $RESULT
}

####
reboot_cluster()
{
    local FSTART=$(date +%s); local RESULT=0
    if [ "$ALL_FOUR_VMS_CREATED" == "YES" ]; then
        $M0VG reload cmu
        $M0VG reload ssu1
        $M0VG reload ssu2
        $M0VG reload client1
    elif [ "$ALL_FOUR_VMS_CREATED" == "NO" ]; then
        $M0VG reload cmu
    else
        print_msg "Some thing has gone wrong with reboot of VMs."
        script_terminate -1
    fi
    vagrant global-status --prune | grep $CL_DIR_TAG
    print_msg "Now waiting for 120 secs for the machines to reboot."
    print_msg "Press ENTER to verify reboot now."
    read -t 120 a

    verify_mount_vms

    FTIME=$(( $(date +%s) - $FSTART ))
    print_msg "Completed $FUNCNAME in $FTIME successfully."
    return $RESULT
}

check_load_on_host()
{
    print_msg "Ideally this script should wait for the system load to reduce"
    print_msg "but for now will only display status & count of running VMs."
    vagrant global-status --prune
}


### main()

if [ $# != 4 ]; then
    print_msg "Invalid Args."
    print_usage
    exit 1
fi
clear
SCRIPT_START=$(date +%s)
HOME_DIR=`pwd`
CL_DIR_TAG=$1
MERO_REF=$2
HARE_USER=$3
HARE_USER_BRANCH=$4
DESTROY_CLUSTER_SCRIPT="`dirname $0`"/destroy-cluster.sh

CL_DIR_PATH=$CL_HOME/$CL_DIR_TAG
M0VG=$CL_DIR_PATH/mero/scripts/m0vg

print_msg "Executing here on machine [`hostname -f`]"
print_msg "Now your Mero & Hare cluster will be created, git cloned, patched/"
print_msg "merged, sources compiled, RPMs will be built, and installed and"
print_msg "tests will be executed"

check_load_on_host

mkdir -p $CL_HOME; cd $CL_HOME

create_cluster
RESULT=$?
if [ $RESULT != 0 ]; then
    print_msg "create_cluster(...) failed. Cannot continue further."
    script_terminate $RESULT
fi

verify_mount_vms

compile_install_mero
RESULT=$?
if [ $RESULT != 0 ]; then
    print_msg "compile_install_mero(...) failed. Cannot continue further."
    script_terminate $RESULT
fi

compile_install_hare
RESULT=$?
if [ $RESULT != 0 ]; then
    print_msg "compile_install_hare(...) failed. Cannot continue further."
    script_terminate $RESULT
fi


start_cluster
RESULT=$?
if [ $RESULT != 0 ]; then
    print_msg "start_cluster(...) failed. Cannot continue further."
    script_terminate $RESULT
fi
reboot_cluster

print_msg "Ready to run tests !!"
run_tests
RESULT=$?

if [ $RESULT != 0 ]; then
    print_msg "run_tests(...) failed. Returned [$RESULT] !!!"
else
    print_msg "run_tests(...) succeeded."
fi
print_msg "DONE ... BBY !!!"
print_msg "CLEANUP will follow !!!"
script_terminate $RESULT

## We donot return her after calling script_terminate(...)
