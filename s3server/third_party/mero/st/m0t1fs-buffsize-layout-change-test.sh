#!/usr/bin/env bash

set -e

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.m0t1fs-writesize-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}
cd $M0_SRC_DIR

echo "Installing Mero services"
sudo scripts/install-mero-service -u
sudo scripts/install-mero-service -l
sudo utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/mero/img -s 1 -c
sudo utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/mero/img -s 1
echo "Start Mero services"
sudo systemctl start mero-mkfs
sudo systemctl start mero-singlenode

touch /mnt/m0t1fs/12345:1
stat /mnt/m0t1fs/12345:1
oldblksize=`stat /mnt/m0t1fs/12345:1 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

dd if=/dev/zero of=/mnt/m0t1fs/12345:1 bs=1048576 count=10
stat /mnt/m0t1fs/12345:1
blksize=`stat /mnt/m0t1fs/12345:1 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

if test $blksize -eq 1048576 -a $oldblksize -ne 1048576; then
	echo "Successfully set IO Block on first write"
else
	echo "IO Block size is not set correctly"
	sudo systemctl stop mero-singlenode
	exit 1
fi

touch /mnt/m0t1fs/12345:2
stat /mnt/m0t1fs/12345:2
oldblksize=`stat /mnt/m0t1fs/12345:2 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

setfattr -n writesize -v "1048576" /mnt/m0t1fs/12345:2
stat /mnt/m0t1fs/12345:2
blksize=`stat /mnt/m0t1fs/12345:2 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

if test $blksize -eq 1048576 -a $oldblksize -ne 1048576; then
	echo "Successfully set IO Block on setfattr"
else
	echo "IO Block size is not set correctly"
	sudo systemctl stop mero-singlenode
	exit 1
fi

echo "Tear down Mero services"
sudo systemctl stop mero-singlenode
sudo scripts/install-mero-service -u

# this msg is used by Jenkins as a test success criteria;
# it should appear on STDOUT
if [ $? -eq 0 ] ; then
    echo "m0d-m0t1fs-writesize: test status: SUCCESS"
fi
