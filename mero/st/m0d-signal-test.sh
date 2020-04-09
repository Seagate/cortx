#!/usr/bin/env bash
set -e

[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}

SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.signal-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. $M0_SRC_DIR/utils/functions # report_and_exit

cd $M0_SRC_DIR

scripts/install-mero-service -u
scripts/install-mero-service -l
utils/m0setup --no-cas -v -c
utils/m0setup --no-cas -v

# update Mero configuration: set specific dir for test artifacts
sed -i "s@.*MERO_LOG_DIR.*@MERO_LOG_DIR=${SANDBOX_DIR}/log@" \
     /etc/sysconfig/mero
sed -i "s@.*MERO_M0D_DATA_DIR.*@MERO_M0D_DATA_DIR=${SANDBOX_DIR}/mero@" \
     /etc/sysconfig/mero

test_for_signal()
{
    local sig=$1
    echo "------------------ Configuring Mero for $sig test ------------------"
    systemctl start mero-mkfs
    systemctl start mero

    echo 'Waiting for ios1 to become active'
    while ! systemctl -q is-active mero-server@ios1 ; do
        sleep 1
    done

    local cursor=$(journalctl --show-cursor -n0 | grep -e '-- cursor:' | sed -e 's/^-- cursor: //')
    while ! journalctl -c "$cursor" -l -u mero-server@ios1 | grep -q 'Press CTRL+C to quit'; do
        sleep 1
    done

    echo "Sending $sig to ios1"
    systemctl -s $sig --kill-who=main kill mero-server@ios1

    if [[ $sig == SIGUSR1 ]] ; then
        sleep 5
    else
        echo "Waiting for ios1 to stop"
        while systemctl -q is-active mero-server@ios1 ; do
            sleep 1
        done
    fi

    if journalctl -c "$cursor" -l -u mero-server@ios1 | grep -Eq 'got signal -?[0-9]+'
    then
        echo "Successfully handled $sig during Mero setup"

        if [[ $sig == SIGUSR1 ]] ; then
            if journalctl -c "$cursor" -l -u mero-server@ios1 | grep -q 'Restarting'
            then
                echo "Wait for Mero restart"
                while ! systemctl -q is-active mero-server@ios1 &&
                      ! systemctl -q is-failed mero-server@ios1
                do
                    sleep 1
                done
            else
                echo "Restarting Mero failed"
                return 1
            fi
        fi
    else
        echo "Failed to handle $sig during Mero setup"
        return 1
    fi

    echo "Stopping Mero"
    systemctl stop mero
    systemctl stop mero-kernel
    return 0
}

# Test for stop
test_for_signal SIGTERM
rc1=$?

# Test for restart
test_for_signal SIGUSR1
rc2=$?

rc=$((rc1 + rc2))
if [ $rc -eq 0 ]; then
    rm -r $SANDBOX_DIR
fi

report_and_exit m0d-signal $rc
