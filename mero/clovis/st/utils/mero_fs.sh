# m0t1fs related helper functions

mount_m0t1fs()
{
	local m0t1fs_mount_dir=$1
	local mountop=

	if [ $# -ne 1 ]
	then
		echo "Usage: mount_m0t1fs <dir>"
		return 1
	fi

	# Create mount directory
	sudo mkdir -p $m0t1fs_mount_dir || {
		echo "Failed to create mount directory."
		return 1
	}

	lsmod | grep -q m0mero || {
		echo "Failed to	mount m0t1fs file system. (m0mero not loaded)"
		return 1
	}

	echo "Mounting file system..."

	local cmd="sudo mount -t m0t1fs \
	    -o profile='$PROF_OPT',confd='$CONFD_EP',$mountop \
	    none $m0t1fs_mount_dir"
	echo $cmd
	eval $cmd || {
		echo "Failed to mount m0t1fs file system."
		return 1
	}
}

umount_m0t1fs()
{
	if [ $# -ne 1 ]
	then
		echo "Usage: umount_m0t1fs <dir>"
		return 1
	fi

	local m0t1fs_mount_dir=$1
	echo "Unmounting file system ..."
	umount $m0t1fs_mount_dir &>/dev/null

	sleep 2

	echo "Cleaning up test directory..."
	rm -rf $m0t1fs_mount_dir &>/dev/null
}

# main entry

# Get the location of this script and look for the command in the
# same directory
dir=`dirname $0`

# Get local address
modprobe lnet
lctl network up &>> /dev/null
LOCAL_NID=`lctl list_nids | head -1`
LOCAL_EP=$LOCAL_NID:12345:33:100
CONFD_EP=$LOCAL_NID:12345:33:100
PROF_OPT='<0x7000000000000001:0>'
PROC_FID='<0x7200000000000001:64>'
cmd_args="$LOCAL_EP $CONFD_EP '$PROF_OPT' '$PROC_FID'"

case "$1" in
	mount)
		echo "mount m0t1fs ... "
		mount_m0t1fs $2
		;;
	umount)
		echo "umount m0t1fs ... "
		umount_m0t1fs $2
		;;
	*)
		echo "Usage: $0 [mount|umount] dir}"
		return 1
esac
