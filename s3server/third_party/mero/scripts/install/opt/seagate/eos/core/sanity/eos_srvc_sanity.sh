#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

## Declare variables

source "/opt/seagate/eos/core/common/eos_error_codes.sh"
source "/opt/seagate/eos/core/common/eos_util_funcs.sh"

RESULT=0
UUID_FILE_CREATED=$NO
EOS_CORE_KERNEL="mero-kernel"
EOS_CORE_KERNEL_SERVICE=$EOS_CORE_KERNEL".service"
ETC_SYSCONFIG_MERO="/etc/sysconfig/mero"

workflow()
{
cat << EOF

This script [$0] will take the following actions:
    1. Verify that the file [$ETC_SYSCONFIG_MERO] exists.
    2. Find the status of [$EOS_CORE_KERNEL_SERVICE].
    3. If [$EOS_CORE_KERNEL_SERVICE] is up and running, the script will exit &
       return 0.
    4. If failed, the script will verify if the file
       [/etc/sysconfig/$EOS_CORE_KERNEL] exists. If not, it will be
       created.
    5. Next it  will attempt to start the [$EOS_CORE_KERNEL_SERVICE]. If
       failed, it will cleanup the $EOS_CORE_KERNEL service states and
       delete the [/etc/sysconfig/$EOS_CORE_KERNEL] file, it it was created,
       and exit with appropriate non-zero error code.
    6. If the [$EOS_CORE_KERNEL_SERVICE] service was started successfully,
       then the service will be shut down, service states cleaned-up, and then
       delete the [/etc/sysconfig/$EOS_CORE_KERNEL] file, it it was created,
       and exit & return 0.

EOF
}

systemctl_action()
{
    local RESULT=0
    CMD=$1
    SYSCTL_EOS_CORE_STATUS=$($CMD)
    RESULT=$?
    if [ $RESULT == 0 ]; then
        msg "EOS Core executing [$CMD] succeeded."
    else
        err "EOS Core executing [$CMD] failed. RESULT [$RESULT]"
        msg "$SYSCTL_EOS_CORE_STATUS"
    fi
    return $RESULT
}

create_temp_uuid_file()
{
    echo "MERO_NODE_UUID='$(uuidgen --time)'" > /etc/sysconfig/$EOS_CORE_KERNEL
    die_if_failed $? "Failed to create [/etc/sysconfig/$EOS_CORE_KERNEL."

    msg "File [/etc/sysconfig/$EOS_CORE_KERNEL] created."
    msg "[$(cat /etc/sysconfig/$EOS_CORE_KERNEL)]"
    UUID_FILE_CREATED=$YES
}

delete_temp_uuid_file()
{
    if [ $UUID_FILE_CREATED == $YES ]; then
        rm /etc/sysconfig/$EOS_CORE_KERNEL
        die_if_failed $? "Failed to rm file [/etc/sysconfig/$EOS_CORE_KERNEL]."

        msg "File [/etc/sysconfig/$EOS_CORE_KERNEL] deleted."
        UUID_FILE_CREATED=$NO
    else
        msg "This file [/etc/sysconfig/$EOS_CORE_KERNEL] was never created by \
        this script."
    fi
}
## main
is_sudoed

workflow

if [ ! -f $ETC_SYSCONFIG_MERO ]; then
    err "File Not found: [$ETC_SYSCONFIG_MERO]"
    die $ERR_ETC_SYSCONFIG_MERO_NOT_FOUND
fi

systemctl_action "systemctl status $EOS_CORE_KERNEL"
RESULT=$?
if [ $RESULT == 0 ]; then
    msg "The $EOS_CORE_KERNEL was found up and running."
    die $ERR_SUCCESS
fi
msg "Status of $EOS_CORE_KERNEL could not be determined. Probing further."

if [ ! -f "/etc/sysconfig/$EOS_CORE_KERNEL" ]; then
    msg "File Not found: [/etc/sysconfig/$EOS_CORE_KERNEL]"
    ## we will create the UUID file here and later delete it
    create_temp_uuid_file
    ## we never return if we fail, so validation of result is not required
else
    msg "File [/etc/sysconfig/$EOS_CORE_KERNEL] exists."
    if [ -z "/etc/sysconfig/$EOS_CORE_KERNEL" ]; then
        msg "File [/etc/sysconfig/$EOS_CORE_KERNEL] exists is empty."
        create_temp_uuid_file
        ## we never return if we fail, so validation of result is not required
    fi
fi

# find out if lnet is running even when
systemctl_action "systemctl status lnet"
LNET_STATUS=$?
systemctl_action "systemctl status mero-trace@kernel.service"
MERO_TRACE_STATUS=$?

systemctl_action "systemctl start $EOS_CORE_KERNEL"
RESULT=$?
if [ $RESULT == 0 ]; then
    systemctl_action "systemctl stop $EOS_CORE_KERNEL"
    RESULT=$?
    if [ $RESULT == 0 ]; then
        msg "The $EOS_CORE_KERNEL was started & stopped successfully."
        if [ $LNET_STATUS != 0 ]; then
            # lnet was not running before we started $EOS_CORE_KERNEL
            # we will stop it down
            msg "Stopping lnet as well."
            systemctl_action "systemctl stop lnet"
        fi

        if [ $MERO_TRACE_STATUS != 0 ]; then
            # mero-trace@kernel.service was not running before we started
            # $EOS_CORE_KERNEL. we will stop it.
            msg "Stopping mero-trace@kernel.service as well."
            systemctl_action "systemctl stop lnet"
        fi

        delete_temp_uuid_file
        die $ERR_SUCCESS
    fi
fi

delete_temp_uuid_file
die $ERR_SRVC_EOS_CORE_STATUS_FAILED
