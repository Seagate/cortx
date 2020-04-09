#!/bin/bash

## Declare all variables
M0_REPO="http://gerrit.mero.colo.seagate.com/mero"
H0_REPO="http://gerrit.mero.colo.seagate.com/halon"
CL_DIR_TAG=""
M0_REF=""
H0_REF=""
CL_DIR_PATH=""
M0VG=""
CLUSTER_CREATE="Y"
CL_HOME="$HOME/virtual_clusters"
print_usage()
{
        echo -e "\n"
        echo -e "Using H0_REPO [$H0_REPO]"
        echo -e "Using M0_REPO [$M0_REPO]"
        echo -e "\n"
        echo -e "./create-cluster.sh <CLUSTER_NAME> <M0_REF> <H0_REF> <CLUSTER_CREATE (Y/N) [Y]>"
        echo -e "\t On master HEAD ./create-cluster.sh CL0001 \"\" \"\""
        echo -e "\t On master with M0_ref ./create-cluster.sh CL0001 \"refs/changes/28/17128/1\" \"\""
        echo -e "\t On master with H0_ref ./create-cluster.sh CL0001 \"\" \"refs/changes/28/17128/1\""
        echo -e "\t On master with M0_REF & H0_ref ./create-cluster.sh CL0001 \"refs/changes/28/17128/1\" \"refs/changes/28/17128/1\""
        echo -e "\t On master HEAD (Reuse the cluster)  ./create-cluster.sh CL0001 \"\" \"\" \"Y\""
        echo -e "\n"
        echo -e "CLUSTER_NAME can be alpha-numeric only. No special characters allowed."
        echo -e "All params is mandatory, can be empty as in \"\" though."
        echo -e "\n"
}

create_cluster()
{
        CLC_START=$(date +%s)
        echo -e "\nUsing H0_REPO [$H0_REPO]"
        echo -e "\nUsing M0_REPO [$M0_REPO]"
        echo -e "\nUsing CL_DIR_PATH [$CL_DIR_PATH].\n"

        if [ -d "$CL_DIR_PATH" ]; then
                echo "Path [$CL_DIR_PATH] exists!! Cleaning up first !!\n"
                $M0VG destroy --force
                rm -rf $CL_DIR_PATH
        fi
        mkdir $CL_DIR_PATH; cd $CL_DIR_PATH

        git clone --recursive $M0_REPO
	#cd mero; git checkout dev/SB/PREP-SCRIPT; cd -;
        git clone --recursive $H0_REPO

        if [ "$M0_REF" != "" ]; then
                echo -e "\nPulling M0_REF [$M0_REF]"
                cd mero
                git pull $M0_REPO $M0_REF
                cd ..
        else
                echo -e "\nM0_REF was not specified."
        fi

        if [ "$H0_REF" != "" ]; then
                echo -e "\nPulling H0_REF [$H0_REF]"
                cd halon
                git pull $H0_REPO $H0_REF
                cd ..
        else
                echo -e "\nH0_REF was not specified."
        fi

        echo -e "\nEditing the m0vg params."
        ./mero/scripts/m0vg env add M0_VM_HOSTNAME_PREFIX=$CL_DIR_TAG
        ./mero/scripts/m0vg env add M0_VM_NAME_PREFIX=$CL_DIR_TAG
        ./mero/scripts/m0vg env add M0_VM_NFS_VERSION=3
        ./mero/scripts/m0vg env add M0_VM_CMU_MEM_MB=8384
        ./mero/scripts/m0vg env add M0_VM_CLIENT_NR=1
        ./mero/scripts/m0vg env add M0_VM_CLIENT_MEM_MB=2046
        ./mero/scripts/m0vg env add M0_VM_SSU_DISKS=6
        ./mero/scripts/m0vg env add M0_VM_SSU_DISK_SIZE_GB=2
        ./mero/scripts/m0vg env add M0_VM_BOX=centos76/dev
        ./mero/scripts/m0vg env add M0_VM_BOX_URL=http://ci-storage.mero.colo.seagate.com/vagrant/centos76/dev
        ./mero/scripts/m0vg status
        echo -e "\n\n\n\n"
        #./mero/scripts/m0vg up cmu ssu1 ssu2 client1
        ./mero/scripts/m0vg up cmu
        cd ..
        CLC_END=$(date +%s)
        CLC_DIFF=$(( $CLC_END - $CLC_START ))
        echo -e "\n\nCluster [$CL_DIR_TAG] created in $CLC_DIFF seconds!!!!\n\n"
}

verify_mount()
{
        TEST_FILE_VM_PATH="/data/TEST_FILE"
        TEST_FILE_HOST_PATH="$CL_DIR_PATH/TEST_FILE"
        VM="$1"
        $M0VG run --vm $VM "touch $TEST_FILE_VM_PATH"
        if [ "$?" == "0" ] && [ -f "$TEST_FILE_HOST_PATH" ]; then
                echo -e "\nMount of /data is verified successfully"
                rm -v "$TEST_FILE_HOST_PATH"
        else
                echo -e "\nMount of [/data] has failed for $VM. Press Enter to try reload now, Ctrl+C to terminate. Will retry after 120 sec."
                read -t 120 a
                $M0VG reload $VM
                verify_mount $VM
        fi
}

### main()
SC_START=$(date +%s)
if [ $# != 3 ]; then
        echo -e "\nInvalid Args."
        print_usage
        exit 1
fi

CL_DIR_TAG=$1
M0_REF=$2
H0_REF=$3
if [ "$#" > 3 ]; then
        SKIP_CLUSTER_CREATE="$4"
fi

CL_DIR_PATH=$CL_HOME/$CL_DIR_TAG
M0VG=$CL_DIR_PATH/mero/scripts/m0vg

mkdir -p $CL_HOME
cd $CL_HOME
## create the cluster now
if [ "$CLUSTER_CREATE" == "Y" ]; then
        echo -e "\n(Re-)Creating the cluster!!!"
        create_cluster
else
        echo -e "\nSkipping the creation of the cluster!!!"
fi

verify_mount cmu
#verify_mount ssu1
#verify_mount ssu2
#verify_mount client1

M0C_START=$(date +%s)
echo -e "\n COMPILATION OF MERO STARTED!!! M0C_START [$M0C_START]!!!\n"
$M0VG run --vm cmu "/data/mero/scripts/provisioning/vmhost/compile-mero.sh"
M0_COMPILATION_RESULT="$?"
M0C_END=$(date +%s)
M0C_DIFF=$(( $M0C_END - $M0C_START ))
if [ "$M0_COMPILATION_RESULT" == "0" ]; then
        echo -e "\nCOMPILATION OF MERO FINISHED SUCCESSFULLY IN $M0C_DIFF SECONDS!!! M0C_END [$M0C_START]!!!\n"
else
        echo -e "\nCOMPILATION OF MERO FINISHED WITH ERRORS IN $M0C_DIFF SECONDS!!! M0C_END [$M0C_START]!!!\n"
fi


H0C_START=$(date +%s)
echo -e "\nCOMPILATION OF HALON STARTED!!! H0C_START [$H0C_START]!!!\n"
$M0VG run --vm cmu "/data/mero/scripts/provisioning/vmhost/compile-halon.sh"
H0_COMPILATION_RESULT="$?"
H0C_END=$(date +%s)
H0C_DIFF=$(( $H0C_END - $H0C_START ))
if [ "$H0_COMPILATION_RESULT" == "0" ]; then
        echo -e "\nCOMPILATION OF HALON FINISHED SUCCESSFULLY IN $H0C_DIFF SECONDS!!! H0C_END [$H0C_START]!!!\n"
else
        echo -e "\nCOMPILATION OF HALON FINISHED WITH ERRORS IN $H0C_DIFF SECONDS!!! H0C_END [$H0C_END]!!!\n"
fi

STCL_START=$(date +%s)
echo -e "\nInitializing the cluster !!!\n"
$M0VG run --vm cmu "/data/mero/scripts/provisioning/vmhost/start-cluster.sh"
STCL_RESULT="$?"
STCL_END=$(date +%s)
STCL_DIFF=$(( $STCL_END - $STCL_START ))
if [ "$STCL_RESULT" == "0" ]; then
        echo -e "\nCOMPILATION OF HALON FINISHED SUCCESSFULLY IN $STCL_DIFF SECONDS!!! STCL_END [$STCL_END]!!!\n"
else
        echo -e "\nCOMPILATION OF HALON FINISHED WITH ERRORS IN $STCL_DIFF SECONDS!!! STCL_END [$STCL_END]!!!\n"
fi
## #######################
# we can have a dir of tests and parse the dir and execute all the tests
# from there
##########################
SC_END=$(date +%s)
SC_DIFF=$(( $SC_END - $SC_START ))
echo -e "\nFull script executed in $SC_DIFF seconds !!!!\n\n"


$M0VG run --vm cmu "/data/halon/scripts/h0 fini"
$M0VG run --vm cmu "/data/mero/scripts/provisioning/vmhost/run-tests.sh"


## after this script ends the jenkins job will call destroy-cluster.sh script to cleanup the cluster
