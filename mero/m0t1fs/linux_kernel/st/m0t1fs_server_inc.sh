conf_ios_device_setup()
{
	local _DDEV_ID=$1
	local _id_count=$2
	local id_count_out=$3
	local _ids="$4"
	local ids_out=$5
	local ddev_id="^d|1:$_DDEV_ID"
	local ddisk_id="^k|1:$_DDEV_ID"
	local ddiskv_id="^j|1:$_DDEV_ID"

	if (($_id_count == 0))
	then
		eval $ids_out="'$ddev_id'"
	else
		eval $ids_out="'$_ids, $ddev_id'"
	fi

	eval $id_count_out=$(( $_id_count + 1 ))

	#dev conf obj
	local ddev_obj="{0x64| (($ddev_id), $(($_DDEV_ID - 1)), 4, 1, 4096, 596000000000, 3, 4, \"/dev/loop$_DDEV_ID\")}"
	#disk conf obj
        local ddisk_obj="{0x6b| (($ddisk_id), $ddev_id, [1: $PVERID])}"
	if (($NR_DISK_FIDS == 0))
	then
		DISK_FIDS="$ddisk_id"
	else
		DISK_FIDS="$DISK_FIDS, $ddisk_id"
	fi
	NR_DISK_FIDS=$(( $NR_DISK_FIDS + 1 ))

	#diskv conf obj
	local ddiskv_obj="{0x6a| (($ddiskv_id), $ddisk_id, [0])}"
	if (($NR_DISKV_FIDS == 0))
	then
		DISKV_FIDS="$ddiskv_id"
	else
		DISKV_FIDS="$DISKV_FIDS, $ddiskv_id"
	fi
	NR_DISKV_FIDS=$(( $NR_DISKV_FIDS + 1 ))

	if (($NR_IOS_DEVS == 0))
	then
		IOS_DEVS="$ddev_obj, \n $ddisk_obj, \n $ddiskv_obj"
	else
		IOS_DEVS="$IOS_DEVS, \n $ddev_obj, \n $ddisk_obj, \n $ddiskv_obj"
	fi
	NR_IOS_DEVS=$(( $NR_IOS_DEVS + 3 ))
	((NR_SDEVS++))
}

mkiosloopdevs()
{
	local ios=$1
	local nr_devs=$2
	local dir=$3
	local adisk=addb-disk.img
	local ddisk=data-disk.img
	local dev_end=0
	local adev_id="^d|1:$ADEV_ID"
	local ids=""
	local id_count=0

	cd $dir || return 1

	ADEV_ID=$(( $ADEV_ID + 1 ))
	dd if=/dev/zero of=$ADEV_ID$adisk bs=1M seek=1M count=1 || return 1
	cat > disks.conf << EOF
Device:
   - id: $ADEV_ID
     filename: `pwd`/$ADEV_ID$adisk
EOF

	dev_end=$(($DDEV_ID + $nr_devs))
	for (( ; DDEV_ID < $dev_end; DDEV_ID++)) ; do
		conf_ios_device_setup $DDEV_ID $id_count id_count "$ids" ids

		dd if=/dev/zero of=$DDEV_ID$ddisk bs=1M seek=1M count=1 ||
			return 1
		if [ ! -e /dev/loop$DDEV_ID ]; then
			create_loop_device $DDEV_ID
		fi
		losetup -d /dev/loop$DDEV_ID &> /dev/null
		losetup /dev/loop$DDEV_ID $DDEV_ID$ddisk
		cat >> disks.conf << EOF
   - id: $DDEV_ID
     filename: /dev/loop$DDEV_ID
EOF
	done

	IOS_DEV_IDS[$(( $ios - 1 ))]="[$id_count: $ids]"

	cd - >/dev/null
	return $?
}

mkiosmddevs()
{
	local nr_ios=$1
	local P=$2
	local nr_dev_per_ios=$(($P / $nr_ios))
	local nr_dev_rem=$(($P % $nr_ios))
	local MDDEV_ID=1
	local nr_dev=$nr_dev_per_ios

	for ((i=0; i < $nr_ios; i++)); do
		local ios=$(( $i + 1 ))
		local ddisk_id="^k|1:$MDDEV_ID"
		local mddiskv_id="^j|2:$MDDEV_ID"

		local mddiskv_obj="{0x6a| (($mddiskv_id), $ddisk_id, [0])}"
		if (($i == 0))
		then
			MDISKV_FIDS="$mddiskv_id"
		else
			MDISKV_FIDS="$MDISKV_FIDS, $mddiskv_id"
		fi

		if (($i == 0))
		then
			IOS_MD_DEVS="$mddiskv_obj"
		else
			IOS_MD_DEVS="$IOS_MD_DEVS, \n $mddiskv_obj"
		fi

		if (($i < $nr_dev_rem))
		then
			nr_dev=$(($nr_dev_per_ios + 1))
		else
			nr_dev=$nr_dev_per_ios
		fi
		MDDEV_ID=$((MDDEV_ID + nr_dev))
	done
}

servers_stop()
{
	prog=$1

	source+eu /etc/rc.d/init.d/functions

	# shutdown services. mds should be stopped last, because
	# other ioservices may have connections to mdservice.
	local pids=$(__pids_pidof $prog)
	echo === pids of services: $pids ===
	echo "Shutting down services one by one. mdservice is the last."
	local delay=5
	local rc=0
	local proc=""

	for pid in $pids; do
		echo -n "----- $pid stopping--------"
		if checkpid $pid 2>&1; then
			# TERM first, then KILL if not dead
			kill -TERM $pid &>/dev/null
			proc=$(ps -o ppid= $pid)
			if [[ $proc -eq $$ ]]; then
				## $pid is spawned by current shell
				wait $pid || rc=$?
			else
				sleep 5
				if checkpid $pid && sleep 5 &&
					checkpid $pid && sleep $delay &&
					checkpid $pid ; then
				kill -KILL $pid &>/dev/null
				usleep 100000
			fi
		fi
	fi
	echo "----- $pid stopped --------"
	done
	return $rc
}

mero_service()
{
	local stride
	local unit_size=$UNIT_SIZE
	local N=$NR_DATA
	local K=$NR_PARITY
	local P=$POOL_WIDTH
	local multiple_pools=$2
	local PROC_FID_CNTR=0x7200000000000001
	local rc=0

	if [ $# -ge 6 ]
	then
		stride=$3
		N=$4
		K=$5
		P=$6
		unit_size=$((stride * 1024))
	fi
	if [ $# -eq 7 ]
	then
		FI_OPTS="-o $7"
	fi

        prog_mkfs="$M0_SRC_DIR/utils/mkfs/m0mkfs"
        prog_start="$M0_SRC_DIR/mero/m0d"
        prog_exec="$M0_SRC_DIR/mero/.libs/lt-m0d"

	if echo $(basename $0) | grep -q 'altogether'; then
		prog_start="${prog_start}-altogether"
		prog_exec="${prog_exec}-altogether"
	fi
	export prog_exec=$prog_exec

	source+eu /etc/rc.d/init.d/functions

	start() {
		NR_IOS_DEVS=0
		NR_SDEVS=0
		DDEV_ID=1
		NR_DISK_FIDS=0
		NR_DISKV_FIDS=0

		local i

		prepare

		# start confd
		DIR=$MERO_M0T1FS_TEST_DIR/confd
		rm -rf $DIR
		mkdir -p $DIR
		ulimit -c unlimited

		local nr_mds=${#MDSEP[*]}
		local nr_ios=${#IOSEP[*]}
		if [ $SINGLE_NODE -eq 1 ] ; then
			nr_ios=1
			nr_mds=1
		fi
		local nr_dev_per_ios=$(($P / $nr_ios))
		local remainder=$(($P % $nr_ios))

		echo "mero_service_start: (N,K,P)=($N,$K,$P) nr_ios=$nr_ios multiple_pools=$multiple_pools"

		#create ios devices
		for ((i=0; i < $nr_ios; i++)) ; do
			local ios=$(( $i + 1 ))
			local nr_dev=$nr_dev_per_ios
			DIR=$MERO_M0T1FS_TEST_DIR/ios$ios
			rm -rf $DIR
			mkdir -p $DIR

			if (($i < $remainder))
			then
				nr_dev=$(($nr_dev_per_ios + 1))
			fi

			mkiosloopdevs $ios $nr_dev $DIR || return 1
		done

		mkiosmddevs $nr_ios $P || return 1
                #create devs for backup pool
		if ((multiple_pools == 1)); then
			DIR=$MERO_M0T1FS_TEST_DIR/ios5
			rm -rf $DIR
			mkdir -p $DIR
			cd $DIR
			dd if=/dev/zero of=data-disk1.img bs=1M seek=1M count=1 ||
				return 1
			dd if=/dev/zero of=data-disk2.img bs=1M seek=1M count=1 ||
				return 1
			dd if=/dev/zero of=data-disk3.img bs=1M seek=1M count=1 ||
				return 1
			losetup -d /dev/loop7 &> /dev/null
			losetup /dev/loop7 data-disk1.img
			losetup -d /dev/loop8 &> /dev/null
			losetup /dev/loop8 data-disk2.img
			losetup -d /dev/loop9 &> /dev/null
			losetup /dev/loop9 data-disk3.img
                cat >> disks.conf << EOF
   - id: 7
     filename: /dev/loop7
   - id: 8
     filename: /dev/loop8
   - id: 9
     filename: /dev/loop9
EOF
		fi

		DIR=$MERO_M0T1FS_TEST_DIR/confd
		CONFDB="$DIR/conf.xc"
		build_conf $N $K $P $multiple_pools | tee $DIR/conf.xc
		common_opts="-D db -S stobs -A linuxstob:addb-stobs \
			     -w $P -m $MAX_RPC_MSG_SIZE \
			     -q $TM_MIN_RECV_QUEUE_LEN -N 100663296 -C 262144 -K 100663296 -k 262144"

		# mkfs for confd server
		opts="$common_opts -T linux -e $XPT:${CONFD_EP%:*:*}:$MKFS_PORTAL:1\
		      -c $CONFDB"
		cmd="cd $DIR && exec $prog_mkfs -F $opts |& tee -a m0mkfs.log"

		echo $cmd
		(eval "$cmd")

		# spawn confd
		local ha_key=$(( $nr_mds + $nr_ios ))
		local confd_key=$(( $ha_key + 1 ))
		local proc_fid="'<"$PROC_FID_CNTR:$confd_key">'"
		opts="$common_opts -f $proc_fid -T linux -e $XPT:$CONFD_EP \
		      -c $CONFDB"
		cmd="cd $DIR && exec $prog_start $opts |& tee -a m0d.log"
		echo $cmd
		(eval "$cmd") &

		# mkfs for ha agent
		DIR=$MERO_M0T1FS_TEST_DIR/ha
		rm -rf $DIR
		mkdir -p $DIR
		opts="$common_opts -T ad -e $XPT:${lnet_nid}:${HA_EP%:*:*}:$MKFS_PORTAL:1 \
		      -c $CONFDB"
		cmd="cd $DIR && exec $prog_mkfs -F $opts |& tee -a m0mkfs.log"
		echo $cmd
		(eval "$cmd")

		#mds mkfs
		for ((i=0; i < $nr_mds; i++)) ; do
			local mds=$(( $i + 1 ))
			DIR=$MERO_M0T1FS_TEST_DIR/mds$mds
			rm -rf $DIR
			mkdir -p $DIR

			tmid=$(echo ${MDSEP[$i]} | cut -d: -f3)
			ulimit -c unlimited
			cmd="cd $DIR && exec \
			$prog_mkfs -F -T ad \
			$common_opts -e $XPT:${lnet_nid}:${MDSEP[$i]%:*:*}:$MKFS_PORTAL:$tmid \
			-c $CONFDB |& tee -a m0mkfs.log"
			echo $cmd
			eval "$cmd"
		done

		#ios mkfs
		for ((i=0; i < $nr_ios; i++)) ; do
			local ios=$(( $i + 1 ))
			proc_fid="'<"$PROC_FID_CNTR:$i">'"
			DIR=$MERO_M0T1FS_TEST_DIR/ios$ios

			tmid=$(echo ${IOSEP[$i]} | cut -d: -f3)
			ulimit -c unlimited
			cmd="cd $DIR && exec \
			$prog_mkfs -F -T $MERO_STOB_DOMAIN \
			$common_opts -e $XPT:${lnet_nid}:${IOSEP[$i]%:*:*}:$MKFS_PORTAL:$tmid  $FI_OPTS \
			-f $proc_fid -c $CONFDB |& tee -a m0mkfs.log"
			echo $cmd
			eval "$cmd"
		done
		if ((multiple_pools == 1)); then
			proc_fid="'<0x720000000000000a:1>'"
			DIR=$MERO_M0T1FS_TEST_DIR/ios5

			tmid=904
			ulimit -c unlimited
			cmd="cd $DIR && exec \
			$prog_mkfs -F -T $MERO_STOB_DOMAIN \
			$common_opts -e $XPT:${lnet_nid}:${IOS_PVER2_EP%:*:*}:$MKFS_PORTAL:$tmid  $FI_OPTS \
			-f $proc_fid -c $CONFDB |& tee -a m0mkfs.log"
			echo $cmd
			eval "$cmd"
		fi

		# spawn ha agent
		proc_fid="'<"$PROC_FID_CNTR:$ha_key">'"
		opts="$common_opts -T ad -e $XPT:${lnet_nid}:$HA_EP \
		      -c $CONFDB -f $proc_fid ${FI_OPT:-} -H ${lnet_nid}:$HA_EP"
		DIR=$MERO_M0T1FS_TEST_DIR/ha
		cmd="cd $DIR && exec $prog_start $opts |& tee -a m0d.log"
		local m0d_log=$DIR/m0d.log
		touch $m0d_log
		echo $cmd
		(eval "$cmd") &

		# Wait for HA agent to start
		while ! grep CTRL $MERO_M0T1FS_TEST_DIR/ha/m0d.log > /dev/null;
		do
			sleep 2
		done
		echo "Mero HA agent started."

		# spawn mds
		for ((i=0; i < $nr_mds; i++)) ; do
			local mds=$(( $i + 1 ))
			local mds_key=$(( $i + $nr_ios ))
			local proc_fid="'<"$PROC_FID_CNTR:$mds_key">'"
			DIR=$MERO_M0T1FS_TEST_DIR/mds$mds

			ulimit -c unlimited

			#let only instance with RMS read config locally
			MDS_CONF=""
			if [ $i -eq 0 ]; then
			    MDS_CONFDB="-c $CONFDB"
			fi

			cmd="cd $DIR && exec \
			$prog_start -T ad \
			$common_opts -e $XPT:${lnet_nid}:${MDSEP[$i]} \
                        -f $proc_fid -H ${lnet_nid}:$HA_EP \
			$MDS_CONFDB |& tee -a m0d.log"
			echo $cmd

			local m0d_log=$DIR/m0d.log
			touch $m0d_log
			(eval "$cmd") &

			# let instance with RMS be the first initialised one;
			# let ha agent initialise along with the mds instance
			if [ $i -eq 0 ]; then
			    sleep 5
			fi

		done

		# spawn ios
		for ((i=0; i < $nr_ios; i++)) ; do
			local ios=$(( $i + 1 ))
			proc_fid="'<"$PROC_FID_CNTR:$i">'"

			DIR=$MERO_M0T1FS_TEST_DIR/ios$ios

			ulimit -c unlimited

			cmd="cd $DIR && exec \
			$prog_start -T $MERO_STOB_DOMAIN \
			$common_opts -e $XPT:${lnet_nid}:${IOSEP[$i]} \
                        -f $proc_fid \
			-H ${lnet_nid}:$HA_EP |& tee -a m0d.log"
			echo $cmd

			local m0d_log=$DIR/m0d.log
			touch $m0d_log
			(eval "$cmd") &
			IOS4_CMD=$cmd
		done
		if ((multiple_pools == 1)); then
			proc_fid="'<0x720000000000000a:1>'"
			DIR=$MERO_M0T1FS_TEST_DIR/ios5
			ulimit -c unlimited
			cmd="cd $DIR && exec \
			$prog_start -T $MERO_STOB_DOMAIN \
			$common_opts -e $XPT:${lnet_nid}:$IOS_PVER2_EP \
                        -f $proc_fid \
			-H ${lnet_nid}:$HA_EP |& tee -a m0d.log"
			echo $cmd

			local m0d_log=$DIR/m0d.log
			touch $m0d_log
			#Save IOS5, to start it again after controller HA event.
			IOS5_CMD=$cmd
			(eval "$cmd") &
		fi

		# Wait for confd to start
		local confd_log=$MERO_M0T1FS_TEST_DIR/confd/m0d.log
		while ! grep CTRL $confd_log > /dev/null; do
			sleep 2
		done
		echo "Mero confd started."


		# Wait for mds to start
		for ((i=0; i < $nr_mds; i++)) ; do
			local mds=$(( $i + 1 ))
			DIR=$MERO_M0T1FS_TEST_DIR/mds$mds
			local m0d_log=$DIR/m0d.log
			while ! grep CTRL $m0d_log > /dev/null; do
				sleep 2
			done
		done
		echo "Mero mdservices started."

		# Wait for ios to start
		for ((i=0; i < $nr_ios; i++)) ; do
			local ios=$(( $i + 1 ))
			DIR=$MERO_M0T1FS_TEST_DIR/ios$ios
			local m0d_log=$DIR/m0d.log
			while ! grep CTRL $m0d_log > /dev/null; do
				sleep 2
			done
		done
		if ((multiple_pools == 1)); then
			while ! grep CTRL $MERO_M0T1FS_TEST_DIR/ios5/m0d.log > /dev/null;
			do
				sleep 2
			done
		fi

		echo "Mero ioservices started."
	}

	stop() {
		servers_stop $prog_exec || rc=$?
		unprepare || rc=$?
		return $rc
	}

	case "$1" in
	    start)
		$1
		;;
	    stop)
		function=$1
		shift
		$function $@ || rc=$?
		echo "Mero services stopped."
		return $rc
		;;
	    *)
		echo "Usage: $0 {start|stop}"
		return 2
	esac
	return $?
}
