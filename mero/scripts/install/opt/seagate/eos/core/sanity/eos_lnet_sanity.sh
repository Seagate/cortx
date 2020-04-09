#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

source "/opt/seagate/eos/core/common/eos_error_codes.sh"
source "/opt/seagate/eos/core/common/eos_util_funcs.sh"

## Declare variables
IP_ADDR=""
LNET_CONF_FILE="/etc/modprobe.d/lnet.conf"
LNET_INSTALLED_COMPONENTS=( "kmod-lustre-client-2.12.3-1.el7.x86_64"
    "lustre-client-2.12.3-1.el7.x86_64")

workflow()
{
cat << EOF

This script [$0] will take the following actions
    1. Verify that Lustre client packages [${LNET_INSTALLED_COMPONENTS[@]}]
       are installed.
    2. Verify that the file [$LNET_CONF_FILE] exists.
    3. Parse the LNET configuration and validate the nids by executing
       [sudo lctl list_nids].
        a. This script will not load the Lustre modules, or verify the loaded
           status.
        b. The script expects the Lustre modules are loaded.
        c. Error is identified by failing [sudo lctl list_nids] command.
    4. On success, it will return Zero.

EOF
}

verify_lnet_installation()
{
    local RESULT=0
    msg "Collecting RPM list"
    RPM_LIST=$(rpm -qa | grep lustre)
    msg "RPM list collected"
    for LNET_COMPONENT in "${LNET_INSTALLED_COMPONENTS[@]}"; do
        if grep -q "$LNET_COMPONENT" <<< "$RPM_LIST"; then
            msg "LNET component [$LNET_COMPONENT] is installed."
        else
            err "LNET component [$LNET_COMPONENT] is not installed."
            RESULT=$ERR_LNET_COMP_NOT_INSTALLED
        fi
    done
    return $RESULT
}

verify_lnet_conf_exists()
{
    local RESULT=0
    if [ ! -f $LNET_CONF_FILE ]; then
        err "LNET conf file not found here $LNET_CONF_FILE"
        RESULT=$ERR_LNET_CONF_FILE_NOT_FOUND
    else
        msg "LNET conf file found here $LNET_CONF_FILE"
        if [ ! -s $LNET_CONF_FILE ]; then
            err "LNET conf file found here $LNET_CONF_FILE is empty."
            RESULT=$ERR_LNET_CONF_FILE_IS_EMPTY
        else
            msg "LNET conf file found here $LNET_CONF_FILE is not empty."
        fi
    fi
    return $RESULT
}

verify_lnet_conf_data()
{
    local RESULT=$ERR_LNET_BAD_CONF_DATA

    while LNET_CONF= read -r CONF_LINE
    do
        if [[ "$CONF_LINE" =~ \#.* ]]; then
            # msg "Comment line: $CONF_LINE"
            continue
        fi
        if [ -z "$CONF_LINE" ]; then
            # msg "Line is empty."
            continue
        fi

        SEP=' ' read -r -a TOK <<< "$CONF_LINE"
        DEVICE=`echo ${TOK[2]} | cut -d "(" -f2 | cut -d ")" -f1`
        msg "Found configured device: [$DEVICE]"
        if [ ! -L "/sys/class/net/"$DEVICE ]; then
            err "Device File [/sys/class/net/$DEVICE] not found."
            RESULT=$ERR_LNET_DEV_FILE_NOT_FOUND
            break
        fi

        IP_ADDR=`ifconfig $DEVICE | awk '/inet /{print substr($2,1)}'`
        if [ "$IP_ADDR" == "" ]; then
            err "Cound not extract IP for Device $DEVICE \n$(ifconfig $DEVICE)"
            RESULT=$ERR_LNET_INVALID_IP
            break
        fi

        msg "Configured device: [$DEVICE] has address [$IP_ADDR]."
        PING_TEST=$(ping -c 3 $IP_ADDR)
        PING_TEST_RESULT=$?
        if [ "$PING_TEST_RESULT" != "0" ]; then
            err "Failed to ping IP_ADDR [$IP_ADDR]\n$PING_TEST"
            RESULT=$ERR_LNET_IP_ADDR_PING_FAILED
            break
        fi

        msg "IP_ADDR [$IP_ADDR] is a valid and reachable IP address"
        RESULT=$ERR_SUCCESS

        ## we are assuming only one Device for LNET configuration
        break

    done < $LNET_CONF_FILE
    return $RESULT
}

verify_lnet_status()
{
    local RESULT=0
    LIST_NIDS=$(sudo lctl list_nids)
    if [ "$LIST_NIDS" != "$IP_ADDR@tcp" ] && [ "$LIST_NIDS" != "$IP_ADDR@o2ib" ]; then
        err "NID [$LIST_NIDS] for [$IP_ADDR] not found."
        LNET_PORT=`netstat -a | grep ":988" | wc -l`
        if [ $LNET_PORT -ge 1 ]; then
            err "Port 988 seems to be in in use \n$(netstat -a | grep \":988\")"
        fi
        RESULT=$ERR_LNET_FOR_IP_ADDR_NOT_FOUND
    else
        msg "NID [$LIST_NIDS] found device [$DEVICE] IP [$IP_ADDR]"
    fi
    return $RESULT
}

## main
RESULT=0

is_sudoed

workflow

verify_lnet_installation
die_if_failed $? "Required Lustre packages [${LNET_INSTALLED_COMPONENTS[@]}] were not found."

verify_lnet_conf_exists
die_if_failed $? "File [$LNET_CONF_FILE] not found or is empty."

verify_lnet_conf_data
die_if_failed $? "Invalid or corrupt configuration found in file [$LNET_CONF_FILE]."

verify_lnet_status
die_if_failed $? "Failed to verify LNET status."

die $RESULT
