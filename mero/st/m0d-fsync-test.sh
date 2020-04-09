#!/usr/bin/env bash
set -e

[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}


SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.fsync-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. $M0_SRC_DIR/utils/functions # report_and_exit

cd $M0_SRC_DIR

echo "Installing Mero services"
scripts/install-mero-service -u
rm -rf /etc/mero
rm -f  /etc/sysconfig/mero
scripts/install-mero-service -l
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/mero/img -s 128 -c
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/mero/img -s 128

# update Mero configuration: turn on fdatasync with '-I' option
sed -i "s/.*MERO_M0D_EXTRA_OPTS.*/MERO_M0D_EXTRA_OPTS='-I'/" \
     /etc/sysconfig/mero

# update Mero configuration: set specific dir for test artifacts
sed -i "s@.*MERO_LOG_DIR.*@MERO_LOG_DIR=${SANDBOX_DIR}/log@" \
     /etc/sysconfig/mero
sed -i "s@.*MERO_M0D_DATA_DIR.*@MERO_M0D_DATA_DIR=${SANDBOX_DIR}/mero@" \
     /etc/sysconfig/mero

echo "Start Mero services"
systemctl start mero-mkfs
systemctl start mero-singlenode

sleep 10 # allow mero to finish its startup

echo "Perform fsync test"
for i in 0:1{0..9}0000; do touch /mnt/m0t1fs/$i & done
for i in $(jobs -p) ; do wait $i ; done

for i in 0:1{0..9}0000; do setfattr -n lid -v 8 /mnt/m0t1fs/$i & done
for i in $(jobs -p) ; do wait $i ; done

for i in 0:1{0..9}0000; do dd if=/dev/zero of=/mnt/m0t1fs/$i \
    bs=8M count=20 conv=fsync & done
for i in $(jobs -p) ; do wait $i ; done

echo "Tear down Mero services"
systemctl stop mero-singlenode
systemctl stop mero-kernel
rc=$?
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/mero/img -s 8 -c
scripts/install-mero-service -u

if [ $rc -eq 0 ]; then
    rm -r $SANDBOX_DIR
fi

report_and_exit m0d-fsync $rc
