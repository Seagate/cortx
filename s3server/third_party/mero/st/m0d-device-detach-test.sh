#!/usr/bin/env bash

[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.device-detach-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

# Number of test iterations
ITER_NR=10

. $M0_SRC_DIR/utils/functions # report_and_exit

# install "mero" Python module required by m0spiel tool
cd $M0_SRC_DIR/utils/spiel
python setup.py install > /dev/null ||
    die 'Cannot install Python "mero" module'
cd $M0_SRC_DIR

echo "Installing Mero services"
scripts/install-mero-service -u
rm -rf /etc/mero
rm -f  /etc/sysconfig/mero
scripts/install-mero-service -l
utils/m0setup -v -P 3 -N 1 -K 1 -i 1 -d /var/mero/img -s 8 -c
utils/m0setup -v -P 3 -N 1 -K 1 -i 1 -d /var/mero/img -s 8

# update Mero configuration: set specific dir for test artifacts
sed -i "s@.*MERO_LOG_DIR.*@MERO_LOG_DIR=${SANDBOX_DIR}/log@" \
     /etc/sysconfig/mero
sed -i "s@.*MERO_M0D_DATA_DIR.*@MERO_M0D_DATA_DIR=${SANDBOX_DIR}/mero@" \
     /etc/sysconfig/mero

echo "Start Mero services"
systemctl start mero-mkfs
systemctl start mero-singlenode
sleep 10 # allow mero to finish its startup

echo "Perform device-detach test"
cd $SANDBOX_DIR

LNET_NID=`lctl list_nids | head -1`
SPIEL_ENDPOINT="$LNET_NID:12345:34:1021"
HA_ENDPOINT="$LNET_NID:12345:45:1"
M0_SPIEL_OPTS="-l $M0_SRC_DIR/mero/.libs/libmero.so --client $SPIEL_ENDPOINT \
               --ha $HA_ENDPOINT"

function spiel_cmd {
    $M0_SRC_DIR/utils/spiel/m0spiel $M0_SPIEL_OPTS <<EOF
fids = {'profile'       : Fid(0x7000000000000001, 0),
        'disk0'         : Fid(0x6b00000000000001, 2)
}

if spiel.cmd_profile_set(str(fids['profile'])):
    sys.exit('cannot set profile {0}'.format(fids['profile']))

if spiel.rconfc_start():
    sys.exit('cannot start rconfc')

device_commands = [('$1', fids['disk0'])]
for command in device_commands:
    try:
        if getattr(spiel, command[0])(*command[1:]) != 0:
            sys.exit("an error occurred while {0} executing, device fid {1}"
                     .format(command[0], command[1]))
    except:
        spiel.rconfc_stop()
        sys.exit("an error occurred while {0} executing, device fid {1}"
                 .format(command[0], command[1]))

spiel.rconfc_stop()
EOF
    return $?
}

rc=0
for I in $(seq 1 $ITER_NR); do
    filename="/mnt/m0t1fs/1:$I"
    echo "Iteration $I of $ITER_NR (file: $filename)"
    touch $filename && setfattr -n lid -v 8 $filename
    rc=$?
    if [ $rc -ne 0 ]; then echo "Cannot create file"; break; fi
    echo "Start I/O"
    dd if=/dev/zero of=$filename bs=1M count=10 >/dev/null 2>&1 &
    dd_pid=$!

    spiel_cmd device_detach
    rc=$?
    echo "device_detach finished with rc=$rc"

    wait $dd_pid
    echo "I/O finished with rc=$? (may fail)"

    if [ $rc -ne 0 ]; then break; fi

    spiel_cmd device_attach
    rc=$?
    echo "device_attach finished with rc=$rc"
    if [ $rc -ne 0 ]; then break; fi
done

echo "Tear down Mero services"
systemctl stop mero-singlenode
systemctl stop mero-kernel
mero_rc=$?
if [ $rc -eq 0 ]; then
    rc=$mero_rc
fi

cd $M0_SRC_DIR
scripts/install-mero-service -u
utils/m0setup -v -P 3 -N 1 -K 1 -i 1 -d /var/mero/img -s 8 -c

if [ $rc -eq 0 ]; then
    rm -r $SANDBOX_DIR
fi

report_and_exit m0d-device-detach $rc
