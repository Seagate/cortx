M0_SRC_DIR=`readlink -f ${BASH_SOURCE[0]}`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}

. $M0_SRC_DIR/utils/functions  # sandbox_init, report_and_exit

[ -n "$SANDBOX_DIR" ] || SANDBOX_DIR=/var/mero/systest-$$
## XXX TODO: Replace `MERO_M0T1FS_TEST_DIR' with `SANDBOX_DIR' everywhere
## and delete the former.
MERO_M0T1FS_TEST_DIR=$SANDBOX_DIR
MERO_TEST_LOGFILE=$SANDBOX_DIR/mero_`date +"%Y-%m-%d_%T"`.log
MERO_M0T1FS_MOUNT_DIR=/tmp/test_m0t1fs_`date +"%d-%m-%Y_%T"`

MERO_MODULE=m0mero
MERO_CTL_MODULE=m0ctl #debugfs interface to control m0mero at runtime

# kernel space tracing parameters
MERO_MODULE_TRACE_MASK='!all'
MERO_TRACE_PRINT_CONTEXT=short
MERO_TRACE_LEVEL=call+

#user-space tracing parameters
export M0_TRACE_IMMEDIATE_MASK="!all"
export M0_TRACE_LEVEL=call+
export M0_TRACE_PRINT_CONTEXT=short

# Note: m0t1fs_dgmode_io.sh refers to value of this variable.
# Hence, do not rename it.
MERO_STOB_DOMAIN="ad -d disks.conf"

IOS_DEVS=""
IOS_MD_DEVS=""
NR_IOS_DEVS=0
IOS_DEV_IDS=

# Number of sdevs in configuration. Used as a counter to assign device id for
# IOS and CAS devices.
NR_SDEVS=0

DISK_FIDS=""
NR_DISK_FIDS=0
DISKV_FIDS=""
MDISKV_FIDS=""
NR_DISKV_FIDS=0

DDEV_ID=1          #data devices
ADEV_ID=100        #addb devices
MDEV_ID=200        #meta-data devices

HA_EP=12345:34:1
CONFD_EP=12345:33:100
MKFS_PORTAL=35

# list of io server end points: e.g., tmid in [900, 999).
IOSEP=(
    12345:33:900   # IOS1 EP
    12345:33:901   # IOS2 EP
    12345:33:902   # IOS3 EP
    12345:33:903   # IOS4 EP
)

IOS_PVER2_EP="12345:33:904"
IOS5_CMD=""       #IOS5 process commandline to spawn it again on Controller event.

IOS4_CMD=""

# list of md server end points tmid in [800, 899)
MDSEP=(
    12345:33:800   # MDS1 EP
#    12345:33:801   # MDS2 EP
#    12345:33:802   # MDS3 EP
)

# Use separate client endpoints for m0repair and m0poolmach utilities
# to avoid network endpoint conflicts (-EADDRINUSE) in-case both the
# utilities are run at the same time, e.g. concurrent i/o with sns repair.
SNS_CLI_EP="12345:33:1000"
POOL_MACHINE_CLI_EP="12345:33:1001"
SNS_QUIESCE_CLI_EP="12345:33:1002"
M0HAM_CLI_EP="12345:33:1003"

POOL_WIDTH=$(expr ${#IOSEP[*]} \* 5)
NR_PARITY=2
NR_DATA=3
UNIT_SIZE=$(expr 1024 \* 1024)

# CAS service starts by default in every process where IOS starts.
# For every CAS service one dedicated "dummy" storage device is assigned.
# CAS service can be switched off completely by setting the variable below to
# some other value.
ENABLE_CAS=1
# Pool version id containing disks serving distributed indices.
DIX_PVERID='^v|1:20'

#MAX_NR_FILES=250
MAX_NR_FILES=20 # XXX temporary workaround for performance issues
TM_MIN_RECV_QUEUE_LEN=16
MAX_RPC_MSG_SIZE=65536
XPT=lnet
PVERID='^v|1:10'
MDPVERID='^v|2:10'
M0T1FS_PROC_ID='<0x7200000000000001:64>'

# Single node configuration.
SINGLE_NODE=0

unload_kernel_module()
{
	mero_module=$MERO_MODULE
	rmmod $mero_module.ko &>> /dev/null
	if [ $? -ne "0" ]
	then
	    echo "Failed to remove $mero_module."
	    return 1
	fi
}

load_kernel_module()
{
	modprobe lnet &>> /dev/null
	lctl network up &>> /dev/null
	lnet_nid=`sudo lctl list_nids | head -1`
	server_nid=${server_nid:-$lnet_nid}

	# see if CONFD_EP was not prefixed with lnet_nid to the moment
	# and pad it in case it was not
	if [ "${CONFD_EP#$lnet_nid:}" = "$CONFD_EP" ]; then
		CONFD_EP=$lnet_nid:$CONFD_EP
	fi

	# Client end point (m0mero module local_addr)
	# last component in this addr will be generated and filled in m0mero.
	LADDR="$lnet_nid:12345:33:"

	mero_module_path=$M0_SRC_DIR
	mero_module=$MERO_MODULE
	lsmod | grep $mero_module &> /dev/null
	if [ $? -eq "0" ]
	then
		echo "Module $mero_module already present."
		echo "Removing existing module for clean test."
		unload_kernel_module || return $?
	fi

        insmod $mero_module_path/$mero_module.ko \
               trace_immediate_mask=$MERO_MODULE_TRACE_MASK \
	       trace_print_context=$MERO_TRACE_PRINT_CONTEXT \
	       trace_level=$MERO_TRACE_LEVEL \
	       node_uuid=${NODE_UUID:-00000000-0000-0000-0000-000000000000} \
               local_addr=$LADDR \
	       tm_recv_queue_min_len=$TM_MIN_RECV_QUEUE_LEN \
	       max_rpc_msg_size=$MAX_RPC_MSG_SIZE
        if [ $? -ne "0" ]
        then
                echo "Failed to insert module \
                      $mero_module_path/$mero_module.ko"
                return 1
        fi
}

load_mero_ctl_module()
{
	echo "Mount debugfs and insert $MERO_CTL_MODULE.ko so as to use fault injection"
	mount -t debugfs none /sys/kernel/debug
	mount | grep debugfs
	if [ $? -ne 0 ]
	then
		echo "Failed to mount debugfs"
		return 1
	fi

	echo "insmod $mero_module_path/$MERO_CTL_MODULE.ko"
	insmod $mero_module_path/$MERO_CTL_MODULE.ko
	lsmod | grep $MERO_CTL_MODULE
	if [ $? -ne 0 ]
	then
		echo "Failed to insert module" \
		     " $mero_module_path/$MERO_CTL_MODULE.ko"
		return 1
	fi
}

unload_mero_ctl_module()
{
	echo "rmmod $mero_module_path/$MERO_CTL_MODULE.ko"
	rmmod $mero_module_path/$MERO_CTL_MODULE.ko
	if [ $? -ne 0 ]; then
		echo "Failed: $MERO_CTL_MODULE.ko could not be unloaded"
		return 1
	fi
}

prepare()
{
	modload_m0gf >& /dev/null
	echo 8 > /proc/sys/kernel/printk
	load_kernel_module || return $?
	sysctl -w vm.max_map_count=30000000 || return $?
}

unprepare()
{
	local rc=0

	sleep 5 # allow pending IO to complete
	if mount | grep m0t1fs > /dev/null; then
		umount $MERO_M0T1FS_MOUNT_DIR || rc=$?
		sleep 2
		rm -rf $MERO_M0T1FS_MOUNT_DIR
	fi

	if lsmod | grep m0mero; then
		unload_kernel_module || rc=$?
	fi
	modunload_m0gf || rc=$?
	## The absence of `sandbox_fini' is intentional.
	return $rc
}

PROF_OPT='<0x7000000000000001:0>'

. `dirname ${BASH_SOURCE[0]}`/common_service_fids_inc.sh

# On cent OS 7 loop device is created during losetup, so no need to call
# create_loop_device().
create_loop_device ()
{
	local dev_id=$1

	mknod -m660 /dev/loop$dev_id b 7 $dev_id
	chown root.disk /dev/loop$dev_id
	chmod 666 /dev/loop$dev_id
}

# Creates pool version objects for storing distributed indices.
# Returns number of created pool version objects.
# Note: Global variables DISK_FIDS and NR_DISK_FIDS are updated by
#       appending DIX disks.
#
# $1 [in]  - pool id to use for distributed indices
# $2 [in]  - pool version id for distributed indices
# $3 [in]  - starting device id for generated storage devices
# $4 [in]  - number of devices to generate in the pool version
# $5 [in]  - FID container to be used in all generated fids
# $6 [out] - variable name to assign list of created conf objects
function dix_pver_build()
{
	local DIX_POOLID=$1
	local DIX_PVERID=$2
	local DIX_DEV_ID=$3
	local DIX_DEVS_NR=$4
	local CONT=$5
	local __res_var=$6
	local DIX_SITEVID="^j|$CONT:1"
	local DIX_RACKVID="^j|$CONT:2"
	local DIX_ENCLVID="^j|$CONT:3"
	local DIX_CTRLVID="^j|$CONT:4"
	local res=""
	local total=0

	# Number of parity units (replication factor) for distributed indices.
	# Calculated automatically as maximum possible parity for the given
	# number of disks.
	local DIX_PARITY=$(((DIX_DEVS_NR - 1) / 2))

	# conf objects for disks
	for ((i=0; i < $DIX_DEVS_NR; i++)); do
		DIX_SDEVID="^d|$CONT:$i"
		DIX_DISKID="^k|$CONT:$i"
		DIX_DISKVID="^j|$CONT:$((100 + $i))"
		DEV_PATH="/dev/loop$((25 + $i))"
		DIX_DISKVIDS="$DIX_DISKVIDS${DIX_DISKVIDS:+,} $DIX_DISKVID"
		DIX_SDEV="{0x64| (($DIX_SDEVID), $((DIX_DEV_ID + $i)), 4, 1, 4096, 596000000000, 3, 4, \"$DEV_PATH\")}"
		DIX_DISK="{0x6b| (($DIX_DISKID), $DIX_SDEVID, [1: $DIX_PVERID])}"
		DIX_DISKV="{0x6a| (($DIX_DISKVID), $DIX_DISKID, [0])}"
		DISK_FIDS="$DISK_FIDS${DISK_FIDS:+,} $DIX_DISKID"
		NR_DISK_FIDS=$((NR_DISK_FIDS + 1))
		res=$res", \n$DIX_SDEV, \n$DIX_DISK, \n$DIX_DISKV"
		total=$(($total + 3))
	done
	# conf objects for DIX pool version
	local DIX_POOL="{0x6f| (($DIX_POOLID), 0, [1: $DIX_PVERID])}"
	local DIX_PVER="{0x76| (($DIX_PVERID), {0| (1, $DIX_PARITY, $DIX_DEVS_NR,
                                                    [5: 0, 0, 0, 0, $DIX_PARITY],
                                                    [1: $DIX_SITEVID])})}"
	local DIX_SITEV="{0x6a| (($DIX_SITEVID), $SITEID, [1: $DIX_RACKVID])}"
	local DIX_RACKV="{0x6a| (($DIX_RACKVID), $RACKID, [1: $DIX_ENCLVID])}"
	local DIX_ENCLV="{0x6a| (($DIX_ENCLVID), $ENCLID, [1: $DIX_CTRLVID])}"
	local DIX_CTRLV="{0x6a| (($DIX_CTRLVID), $CTRLID, [$DIX_DEVS_NR: $DIX_DISKVIDS])}"
	res=$res", \n$DIX_POOL, \n$DIX_PVER, \n$DIX_SITEV, \n$DIX_RACKV"
	res=$res", \n$DIX_ENCLV, \n$DIX_CTRLV"
	total=$(($total + 6))
	eval $__res_var="'$res'"
	return $total
}

###############################
# globals: MDSEP[], IOSEP[], server_nid
###############################
function build_conf()
{
	local nr_data_units=$1
	local nr_parity_units=$2
	local pool_width=$3
	local multiple_pools=$4
	local ioservices=("${!5}")
	local mdservices=("${!6}")
	local DIX_FID_CON='20'
	local DIX_PVER_OBJS=
	local IOS_OBJS_NR=0
	local PVER1_OBJ_COUNT=0
	local PVER1_OBJS=
	local node_count=1
	local pool_count=1
	local pvers_count=1
	local site_count=1
	local PROC_FID_CONT='^r|1'
	local MD_REDUNDANCY=1
	local m0t1fs_ep="$lnet_nid:12345:33:1"
	local nr_ios=${#IOSEP[*]}

	if [ $SINGLE_NODE -eq 1 ] ; then
		nr_ios=1
	fi

	if [ -z "$ioservices" ]; then
		if [ $SINGLE_NODE -eq 1 ]; then
			ioservices=("${IOSEP[0]}")
		else
			ioservices=("${IOSEP[@]}")
		fi
		#for ((i = 0; i < ${#ioservices[*]}; i++)); do
		for ((i = 0; i < $nr_ios; i++)); do
			ioservices[$i]=${server_nid}:${ioservices[$i]}
		done
	fi

	if [ -z "$mdservices" ]; then
		if [ $SINGLE_NODE -eq 1 ]; then
			mdservices=("${MDSEP[0]}")
		else
			mdservices=("${MDSEP[@]}")
		fi
		for ((i = 0; i < ${#mdservices[*]}; i++)); do
			mdservices[$i]=${server_nid}:${mdservices[$i]}
		done
	fi

	if [ $multiple_pools -eq "1" ]; then
		MD_REDUNDANCY=2
	fi;
	# prepare configuration data
	local CONFD_ENDPOINT="\"${mdservices[0]%:*:*}:33:100\""
	local HA_ENDPOINT="\"${mdservices[0]%:*:*}:34:1\""
	local  ROOT='^t|1:0'
	local  PROF='^p|1:0'
	local  NODE='^n|1:2'
	local CONFD="$CONF_FID_CON:0"
	local HA_SVC_ID='^s|1:6'
	local FIS_SVC_ID='^s|1:7'
	local  SITEID='^S|1:6'
	local  RACKID='^a|1:6'
	local  ENCLID='^e|1:7'
	local  CTRLID='^c|1:8'
	local  POOLID='^o|1:9'
	local  MDPOOLID='^o|2:9'
	local  PVERFID1='^v|0x40000000000001:11'
	local  PVERFID2='^v|0x40000000000001:12'
	#"pool_width" number of objv created for devv conf objects
	local  SITEVID="^j|1:$(($pool_width + 1))"
	local  RACKVID="^j|1:$(($pool_width + 2))"
	local  ENCLVID="^j|1:$(($pool_width + 3))"
	local  CTRLVID="^j|1:$(($pool_width + 4))"
	#mdpool objects
	local  MDSITEVID="^j|2:$(($pool_width + 1))"
	local  MDRACKVID="^j|2:$(($pool_width + 2))"
	local  MDENCLVID="^j|2:$(($pool_width + 3))"
	local  MDCTRLVID="^j|2:$(($pool_width + 4))"

	local M0T1FS_RMID="^s|1:101"
	local M0T1FS_PROCID="^r|1:100"

	local NODES="$NODE"
	local POOLS="$POOLID"
	local PVER_IDS="$PVERID"
	local SITES="$SITEID"
	local PROC_NAMES
	local PROC_OBJS
	local M0D=0
	local M0T1FS_RM="{0x73| (($M0T1FS_RMID), @M0_CST_RMS, [1: \"${m0t1fs_ep}\"], [0], [0])}"
	local M0T1FS_PROC="{0x72| (($M0T1FS_PROCID), [1:3], 0, 0, 0, 0, \"${m0t1fs_ep}\", [1: $M0T1FS_RMID])}"
	PROC_OBJS="$PROC_OBJS${PROC_OBJS:+, }\n  $M0T1FS_PROC"
	PROC_NAMES="$PROC_NAMES${PROC_NAMES:+, }$M0T1FS_PROCID"

	local i
	for ((i=0; i < ${#ioservices[*]}; i++, M0D++)); do
	    local IOS_NAME="$IOS_FID_CON:$i"
	    local ADDB_NAME="$ADDB_IO_FID_CON:$i"
	    local SNS_REP_NAME="$SNSR_FID_CON:$i"
	    local SNS_REB_NAME="$SNSB_FID_CON:$i"
	    local iosep="\"${ioservices[$i]}\""
	    local IOS_OBJ="{0x73| (($IOS_NAME), @M0_CST_IOS, [1: $iosep], [0], ${IOS_DEV_IDS[$i]})}"
	    local ADDB_OBJ="{0x73| (($ADDB_NAME), @M0_CST_ADDB2, [1: $iosep], [0], [0])}"
	    local SNS_REP_OBJ="{0x73| (($SNS_REP_NAME), @M0_CST_SNS_REP, [1: $iosep], [0], [0])}"
	    local SNS_REB_OBJ="{0x73| (($SNS_REB_NAME), @M0_CST_SNS_REB, [1: $iosep], [0], [0])}"
	    local RM_NAME="$RMS_FID_CON:$M0D"
	    local RM_OBJ="{0x73| (($RM_NAME), @M0_CST_RMS, [1: $iosep], [0], [0])}"
	    local NAMES_NR=5
	    if [ $ENABLE_CAS -eq 1 ] ; then
	        local DIX_REP_NAME="$DIXR_FID_CON:$i"
	        local DIX_REB_NAME="$DIXB_FID_CON:$i"
	        local CAS_NAME="$CAS_FID_CON:$i"
	        local CAS_OBJ="{0x73| (($CAS_NAME), @M0_CST_CAS, [1: $iosep], [0], [1: ^d|$DIX_FID_CON:$i])}"
	        local DIX_REP_OBJ="{0x73| (($DIX_REP_NAME), @M0_CST_DIX_REP, [1: $iosep], [0], [0])}"
	        local DIX_REB_OBJ="{0x73| (($DIX_REB_NAME), @M0_CST_DIX_REB, [1: $iosep], [0], [0])}"
                local CAS_OBJS="$CAS_OBJ, \n  $DIX_REP_OBJ, \n  $DIX_REB_OBJ"
	        NAMES_NR=8
	    fi

	    PROC_NAME="$PROC_FID_CONT:$M0D"
	    IOS_NAMES[$i]="$IOS_NAME, $ADDB_NAME, $SNS_REP_NAME, $SNS_REB_NAME, \
	                   $RM_NAME${CAS_NAME:+,} $CAS_NAME, $DIX_REP_NAME, $DIX_REB_NAME"
	    PROC_OBJ="{0x72| (($PROC_NAME), [1:3], 0, 0, 0, 0, $iosep, [$NAMES_NR: ${IOS_NAMES[$i]}])}"
	    IOS_OBJS="$IOS_OBJS${IOS_OBJS:+, }\n  $IOS_OBJ, \n  $ADDB_OBJ, \
	               \n $SNS_REP_OBJ, \n  $SNS_REB_OBJ, \n $RM_OBJ${CAS_OBJS:+, \n} $CAS_OBJS"
	    PROC_OBJS="$PROC_OBJS${PROC_OBJS:+, }\n  $PROC_OBJ"
	    # +1 here for process object
	    IOS_OBJS_NR=$(($IOS_OBJS_NR + $NAMES_NR + 1))
	    PROC_NAMES="$PROC_NAMES${PROC_NAMES:+, }$PROC_NAME"
	done

	for ((i=0; i < ${#mdservices[*]}; i++, M0D++)); do
	    local MDS_NAME="$MDS_FID_CON:$i"
	    local ADDB_NAME="$ADDB_MD_FID_CON:$i"
	    local mdsep="\"${mdservices[$i]}\""
	    local MDS_OBJ="{0x73| (($MDS_NAME), @M0_CST_MDS, [1: $mdsep], [0], [0])}"
	    local ADDB_OBJ="{0x73| (($ADDB_NAME), @M0_CST_ADDB2, [1: $mdsep], [0], [0])}"
	    local RM_NAME="$RMS_FID_CON:$M0D"
	    local RM_OBJ="{0x73| (($RM_NAME), @M0_CST_RMS, [1: $mdsep], [0], [0])}"

	    PROC_NAME="$PROC_FID_CONT:$M0D"
	    MDS_NAMES[$i]="$MDS_NAME, $ADDB_NAME, $RM_NAME"
	    MDS_OBJS="$MDS_OBJS${MDS_OBJS:+,} \n $MDS_OBJ, \n  $ADDB_OBJ, \n  $RM_OBJ"
	    PROC_OBJ="{0x72| (($PROC_NAME), [1:3], 0, 0, 0, 0, $mdsep, [3: ${MDS_NAMES[$i]}])}"
	    PROC_OBJS="$PROC_OBJS, \n $PROC_OBJ"
	    PROC_NAMES="$PROC_NAMES, $PROC_NAME"
	done

	if [ $ENABLE_CAS -eq 1 ] ; then
		local DIX_POOLID="^o|$DIX_FID_CON:1"
		local START_DEV_ID=$NR_SDEVS
		IMETA_PVER=$DIX_PVERID

		# XXX: Hack to make st/m0t1fs_pool_version_assignment.sh work.
		# Test fails if CAS device identifiers are between device
		# identifiers of IOS devices in pool1 and pool2.
		# 3 here is a number of devices in pool2.
		if ((multiple_pools == 1)); then
			START_DEV_ID=$((START_DEV_ID + 3))
		fi
		dix_pver_build $DIX_POOLID $DIX_PVERID $START_DEV_ID \
			${#ioservices[*]} $DIX_FID_CON DIX_PVER_OBJS
		DIX_PVER_OBJ_COUNT=$?
		PVER_IDS="$PVER_IDS, $DIX_PVERID"
		pvers_count=$(($pvers_count + 1))
		POOLS="$POOLS, $DIX_POOLID"
		pool_count=$(($pool_count + 1))
	else
		IMETA_PVER='(0,0)'
		DIX_PVER_OBJ_COUNT=0
		DIX_PVER_OBJS=""
	fi

	# Add a separate mdpool with a disk from each of the ioservice
	local nr_ios=${#ioservices[*]}
	local MDPOOL="{0x6f| (($MDPOOLID), 0, [1: $MDPVERID])}"
	local MDPVER="{0x76| (($MDPVERID), {0| ($nr_ios, 0, $nr_ios, [5: 0, 0, 0, 0, 1], [1: $MDSITEVID])})}"
	local MDSITEV="{0x6a| (($MDSITEVID), $SITEID, [1: $MDRACKVID])}"
	local MDRACKV="{0x6a| (($MDRACKVID), $RACKID, [1: $MDENCLVID])}"
	local MDENCLV="{0x6a| (($MDENCLVID), $ENCLID, [1: $MDCTRLVID])}"
	local MDCTRLV="{0x6a| (($MDCTRLVID), $CTRLID, [${#ioservices[*]}: $MDISKV_FIDS])}"

	MD_OBJS=", \n$MDPOOL, \n$MDPVER, \n$MDSITEV, \n$MDRACKV, \n$MDENCLV"
        MD_OBJS="$MD_OBJS, \n$MDCTRLV, \n$IOS_MD_DEVS"
        MD_OBJ_COUNT=$((5 + ${#ioservices[*]}))
	PVER_IDS="$PVER_IDS, $MDPVERID"
	pvers_count=$(($pvers_count + 1))
	POOLS="$POOLS, $MDPOOLID"
	pool_count=$(($pool_count + 1))

	PROC_NAME="$PROC_FID_CONT:$((M0D++))"
	RM_NAME="$RMS_FID_CON:$M0D"
	RM_OBJ="{0x73| (($RM_NAME), @M0_CST_RMS, [1: $HA_ENDPOINT], [0], [0])}"
	RM_OBJS="$RM_OBJS${RM_OBJS:+,} \n $RM_OBJ"
	PROC_OBJ="{0x72| (($PROC_NAME), [1:3], 0, 0, 0, 0, "${HA_ENDPOINT}",
                          [3: $HA_SVC_ID, $FIS_SVC_ID, $RM_NAME])}"
	PROC_OBJS="$PROC_OBJS, \n $PROC_OBJ"
	PROC_NAMES="$PROC_NAMES, $PROC_NAME"
	PROC_NAME="$PROC_FID_CONT:$((M0D++))"
	RM_NAME="$RMS_FID_CON:$M0D"
	RM_OBJ="{0x73| (($RM_NAME), @M0_CST_RMS, [1: $CONFD_ENDPOINT], [0], [0])}"
	RM_OBJS="$RM_OBJS${RM_OBJS:+,} \n $RM_OBJ"
	PROC_OBJ="{0x72| (($PROC_NAME), [1:3], 0, 0, 0, 0, "${CONFD_ENDPOINT}",
                          [2: $CONFD, $RM_NAME])}"
	PROC_OBJS="$PROC_OBJS, \n $PROC_OBJ"
	PROC_NAMES="$PROC_NAMES, $PROC_NAME"
	local SITE="{0x53| (($SITEID), [1: $RACKID], [$pvers_count: $PVER_IDS])}"
	local RACK="{0x61| (($RACKID), [1: $ENCLID], [$pvers_count: $PVER_IDS])}"
	local ENCL="{0x65| (($ENCLID), [1: $CTRLID], [$pvers_count: $PVER_IDS])}"
	local CTRL="{0x63| (($CTRLID), $NODE, [$NR_DISK_FIDS: $DISK_FIDS],
                            [$pvers_count: $PVER_IDS])}"
	local POOL="{0x6f| (($POOLID), 0, [3: $PVERID, $PVERFID1, $PVERFID2])}"
	local PVER="{0x76| (($PVERID), {0| ($nr_data_units, $nr_parity_units,
                                            $pool_width,
                                            [5: 0, 0, 0, 0, $nr_parity_units],
                                            [1: $SITEVID])})}"
	local PVER_F1="{0x76| (($PVERFID1), {1| (0, $PVERID, [5: 0, 0, 0, 0, 1])})}"
	local PVER_F2="{0x76| (($PVERFID2), {1| (1, $PVERID, [5: 0, 0, 0, 0, 2])})}"
	local SITEV="{0x6a| (($SITEVID), $SITEID, [1: $RACKVID])}"
	local RACKV="{0x6a| (($RACKVID), $RACKID, [1: $ENCLVID])}"
	local ENCLV="{0x6a| (($ENCLVID), $ENCLID, [1: $CTRLVID])}"
	local CTRLV="{0x6a| (($CTRLVID), $CTRLID, [$NR_DISKV_FIDS: $DISKV_FIDS])}"

	if ((multiple_pools == 1)); then
		# IDs for another pool version to test assignment
		# of pools to new objects.
		local  NODEID1='^n|10:1'
		local  PROCID1='^r|10:1'
		local  IO_SVCID1='^s|10:1'
		local  ADDB_SVCID1='^s|10:2'
		local  REP_SVCID1='^s|10:3'
		local  REB_SVCID1='^s|10:4'
		local  SDEVID1='^d|10:1'
		local  SDEVID2='^d|10:2'
		local  SDEVID3='^d|10:3'
		local  SITEID1='^S|10:1'
		local  RACKID1='^a|10:1'
		local  ENCLID1='^e|10:1'
		local  CTRLID1='^c|10:1'
		local  DISKID1='^k|10:1'
		local  DISKID2='^k|10:2'
		local  DISKID3='^k|10:3'
		local  POOLID1='^o|10:1'
		local  PVERID1='^v|10:1'
		local  SITEVID1='^j|11:1'
		local  RACKVID1='^j|10:1'
		local  ENCLVID1='^j|10:2'
		local  CTRLVID1='^j|10:3'
		local  DISKVID1='^j|10:4'
		local  DISKVID2='^j|10:5'
		local  DISKVID3='^j|10:6'
		local  IOS_EP="\"${server_nid}:$IOS_PVER2_EP\""
		# conf objects for another pool version to test assignment
		# of pools to new objects.
		local NODE1="{0x6e| (($NODEID1), 16000, 2, 3, 2, [1: $PROCID1])}"
		local PROC1="{0x72| (($PROCID1), [1:3], 0, 0, 0, 0, $IOS_EP, [4: $IO_SVCID1, $ADDB_SVCID1, $REP_SVCID1, $REB_SVCID1])}"
		local IO_SVC1="{0x73| (($IO_SVCID1), @M0_CST_IOS, [1: $IOS_EP], [0], [3: $SDEVID1, $SDEVID2, $SDEVID3])}"
		local ADDB_SVC1="{0x73| (($ADDB_SVCID1), @M0_CST_ADDB2, [1: $IOS_EP], [0], [0])}"
		local REP_SVC1="{0x73| (($REP_SVCID1), @M0_CST_SNS_REP, [1: $IOS_EP], [0], [0])}"
		local REB_SVC1="{0x73| (($REB_SVCID1), @M0_CST_SNS_REB, [1: $IOS_EP], [0], [0])}"
		local SDEV1="{0x64| (($SDEVID1), $((NR_SDEVS++)), 4, 1, 4096, 596000000000, 3, 4, \"/dev/loop7\")}"
		local SDEV2="{0x64| (($SDEVID2), $((NR_SDEVS++)), 4, 1, 4096, 596000000000, 3, 4, \"/dev/loop8\")}"
		local SDEV3="{0x64| (($SDEVID3), $((NR_SDEVS++)), 4, 1, 4096, 596000000000, 3, 4, \"/dev/loop9\")}"
		local SITE1="{0x53| (($SITEID1), [1: $RACKID1], [1: $PVERID1])}"
		local RACK1="{0x61| (($RACKID1), [1: $ENCLID1], [1: $PVERID1])}"
		local ENCL1="{0x65| (($ENCLID1), [1: $CTRLID1], [1: $PVERID1])}"
		local CTRL1="{0x63| (($CTRLID1), $NODEID1, [3: $DISKID1, $DISKID2, $DISKID3], [1: $PVERID1])}"
                local DISK1="{0x6b| (($DISKID1), $SDEVID1, [1: $PVERID1])}"
                local DISK2="{0x6b| (($DISKID2), $SDEVID2, [1: $PVERID1])}"
                local DISK3="{0x6b| (($DISKID3), $SDEVID3, [1: $PVERID1])}"

		local POOL1="{0x6f| (($POOLID1), 0, [1: $PVERID1])}"
		local PVER1="{0x76| (($PVERID1), {0| (1, 1, 3, [5: 0, 0, 0, 0, 1], [1: $SITEVID1])})}"
		local SITEV1="{0x6a| (($SITEVID1), $SITEID1, [1: $RACKVID1])}"
		local RACKV1="{0x6a| (($RACKVID1), $RACKID1, [1: $ENCLVID1])}"
		local ENCLV1="{0x6a| (($ENCLVID1), $ENCLID1, [1: $CTRLVID1])}"
		local CTRLV1="{0x6a| (($CTRLVID1), $CTRLID1, [3: $DISKVID1, $DISKVID2, $DISKVID3])}"
		local  DISKV1="{0x6a| (($DISKVID1), $DISKID1, [0])}"
		local  DISKV2="{0x6a| (($DISKVID2), $DISKID2, [0])}"
		local  DISKV3="{0x6a| (($DISKVID3), $DISKID3, [0])}"
		PVER1_OBJS=",\n$NODE1, \n$PROC1, \n$IO_SVC1, \n$ADDB_SVC1,
                            $REP_SVC1, \n$REB_SVC1, \n$SDEV1, \n$SDEV2,
                            $SDEV3, \n$SITE1, \n$RACK1, \n$ENCL1, \n$CTRL1,
                            $DISK1, \n$DISK2, \n$DISK3, \n$POOL1, \n$PVER1,
                            $SITEV1, \n$RACKV1, $ENCLV1, \n$CTRLV1, \n$DISKV1,
                            $DISKV2, \n$DISKV3"
		PVER1_OBJ_COUNT=25
		((++node_count))
		((++site_count))
		((++pool_count))

		NODES="$NODES, $NODEID1"
		POOLS="$POOLS, $POOLID1"
		SITES="$SITES, $SITEID1"
		# Total 25 objects for this other pool version
	fi

 # Here "15" configuration objects includes services excluding ios & mds,
 # pools, racks, enclosures, controllers and their versioned objects.
	echo -e "
[$(($IOS_OBJS_NR + $((${#mdservices[*]} * 5)) + $NR_IOS_DEVS + 19
    + $MD_OBJ_COUNT + $PVER1_OBJ_COUNT + 5 + $DIX_PVER_OBJ_COUNT)):
  {0x74| (($ROOT), 1, (11, 22), $MDPOOLID, $IMETA_PVER, $MD_REDUNDANCY,
	  [1: \"$pool_width $nr_data_units $nr_parity_units\"],
	  [$node_count: $NODES],
	  [$site_count: $SITES],
	  [$pool_count: $POOLS],
	  [1: $PROF], [0])},
  {0x70| (($PROF), [$pool_count: $POOLS])},
  {0x6e| (($NODE), 16000, 2, 3, 2, [$(($M0D + 1)): ${PROC_NAMES[@]}])},
  $PROC_OBJS,
  {0x73| (($CONFD), @M0_CST_CONFD, [1: $CONFD_ENDPOINT], [0], [0])},
  {0x73| (($HA_SVC_ID), @M0_CST_HA, [1: $HA_ENDPOINT], [0], [0])},
  {0x73| (($FIS_SVC_ID), @M0_CST_FIS, [1: $HA_ENDPOINT], [0], [0])},
  $M0T1FS_RM,
  $MDS_OBJS,
  $IOS_OBJS,
  $RM_OBJS,
  $IOS_DEVS,
  $SITE,
  $RACK,
  $ENCL,
  $CTRL,
  $POOL,
  $PVER,
  $PVER_F1,
  $PVER_F2,
  $SITEV,
  $RACKV,
  $ENCLV,
  $CTRLV $MD_OBJS $PVER1_OBJS $DIX_PVER_OBJS]"
}

service_eps_get()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local service_eps

	if [ $SINGLE_NODE -eq 1 ] ; then
		service_eps=(
			"$lnet_nid:${IOSEP[0]}"
			"$lnet_nid:${HA_EP}"
		)
	else
		service_eps=(
			"$lnet_nid:${IOSEP[0]}"
			"$lnet_nid:${IOSEP[1]}"
			"$lnet_nid:${IOSEP[2]}"
			"$lnet_nid:${IOSEP[3]}"
			"$lnet_nid:${HA_EP}"
		)
	fi

	echo "${service_eps[*]}"
}

service_eps_with_m0t1fs_get()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local service_eps=$(service_eps_get)

	service_eps+=("$lnet_nid:12345:33:1")

	# Return list of endpoints
	echo "${service_eps[*]}"
}

service_cas_eps_with_m0tifs_get()
{
	local lnet_nid=`sudo lctl list_nids | head -1`
	local service_eps=(
		"$lnet_nid:${CASEP[0]}"
		"$lnet_nid:${CASEP[1]}"
		"$lnet_nid:${CASEP[2]}"
		"$lnet_nid:${CASEP[3]}"
		"$lnet_nid:${HA_EP}"
	)
	# Return list of endpoints
	echo "${service_eps[*]}"
}

# input parameters:
# (i) ha_msg_nvec in yaml format (see utils/m0hagen)
# (ii) list of remote endpoints
# (iii) local endpoint
send_ha_msg_nvec()
{
	local ha_msg_nvec=$1
	local remote_eps=($2)
	local local_ep=$3

	# Complete the message for m0hagen
	local ha_msg_yaml='!ha_msg
fid: !fid [0,0]
process: ^r|0.0
service: ^s|0.0
data:'
	ha_msg_nvec=`echo "$ha_msg_nvec" | sed 's/^/  /g'`
	ha_msg_yaml+="$ha_msg_nvec"

	# Convert a yaml message to its xcode representation
	cmd_xcode="echo \"$ha_msg_yaml\" | $M0_SRC_DIR/utils/m0hagen"
	# Check for errors in the message format
	(eval "$cmd_xcode > /dev/null")
	if [ $? -ne 0 ]; then
		echo "m0hagen can not convert message:"
		echo "$ha_msg_yaml"
		return 1
	fi

	for ep in "${remote_eps[@]}"; do
		cmd="$cmd_xcode | $M0_SRC_DIR/utils/m0ham -v -s $local_ep $ep"
		echo "$cmd"
		local xcode=`eval "$cmd"`
		if [ $? -ne 0 ]; then
			echo "m0ham failed to send a message"
			return 1
		fi
		if [ ! -z "$xcode" ]; then
			echo "Got reply (xcode):"
			echo "$xcode" | head -c 100
			echo
			echo "Decoded reply:"
			echo "$xcode" | $M0_SRC_DIR/utils/m0hagen -d
		fi
	done
}

# input parameters:
# (i) list of fids
# (ii) state in string representation
# (iii) list of remote endpoints
# (iv) local endpoint
send_ha_events()
{
	local fids=($1)
	local state=$2
	local remote_eps=($3)
	local local_ep=$4

	local yaml='!ha_nvec_set
ignore_same_state: false
notes:'
	for fid in "${fids[@]}"; do
		# Convert fid to m0hagen's format
		fid=`echo "$fid" | tr : .`
		yaml="$yaml
  - {fid: $fid, state: $state}"
	done

	send_ha_msg_nvec "$yaml" "${remote_eps[*]}" "$local_ep"
}

# input parameters:
# (i) list of fids
# (ii) state in string representation
send_ha_events_default()
{
	local fids=($1)
	local state=$2

	# Use default endpoints
	local lnet_nid=`sudo lctl list_nids | head -1`
	local ha_ep="$lnet_nid:$HA_EP"
	local local_ep="$lnet_nid:$M0HAM_CLI_EP"

	send_ha_events "${fids[*]}" "$state" "$ha_ep" "$local_ep"
}

# input parameters:
# (i) list of fids
# (ii) list of remote endpoints
# (iii) local endpoint
request_ha_state()
{
	local fids=($1)
	local remote_eps=($2)
	local local_ep=$3

	local yaml='!ha_nvec_get
get_id: 100
ignore_same_state: false
fids:'
	for fid in "${fids[@]}"; do
		# Convert fid to m0hagen's format
		fid=`echo "$fid" | tr : .`
		yaml="$yaml
  - $fid"
	done

	send_ha_msg_nvec "$yaml" "${remote_eps[*]}" "$local_ep"
}

function run()
{
	echo "# $*"
	eval $*
}
