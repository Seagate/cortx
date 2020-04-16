#!/usr/bin/env bash

MERO_CONF_FILE="/opt/seagate/eos/core/conf/mero.conf"
ETC_SYSCONFIG_MERO="/etc/sysconfig/mero"

source "/opt/seagate/eos/core/common/eos_error_codes.sh"
source "/opt/seagate/eos/core/common/eos_util_funcs.sh"

help()
{
cat << EOF
    CAUTION !!!

    The [$0] script can be invoked by the provisioner, or for
    modifying specific key value pairs in [$ETC_SYSCONFIG_MERO].
    Should be invoked with sudo.

    Usage:
    sudo $0 [ACTION] ARGS
        ACTION:  [ACTION -g|-e|-d|-s] [ARGS]
            ACTION    : Invokes the provisioner action, when
                        [sudo m0provision config] is invoked
            ACTION -g : get the value & state (enabled | disabled) for key
                        sudo $0 -g MERO_M0D_DATA_UNITS /etc/sysconfig/mero
            ACTION -e : enable the key
                        sudo $0 -e MERO_M0D_DATA_UNITS /etc/sysconfig/mero
            ACTION -d : disable the key
                        sudo $0 -d MERO_M0D_DATA_UNITS /etc/sysconfig/mero
            ACTION -s : set the value for key, also enables the key
                        sudo $0 -s MERO_M0D_DATA_UNITS 0 /etc/sysconfig/mero
EOF
}

workflow()
{
cat << EOF
    This script [$0] will take the following actions when executed by the
    m0provision:
        1. Verify that the script is invoked for physical cluster.
        2. If not physical cluster (i.e., virtual cluster), the script will
           terminate with ERR_NOT_IMPLEMENTED [$ERR_NOT_IMPLEMENTED].
        3. If physical cluster, the script will read the
           /opt/seagate/eos/core/conf/mero.conf and update the params in the
           /etc/sysconfig/mero.

EOF
}

file_exists_else_die() # ARG1 [FILE]
{
    if [ $# != 1 ]; then
        err "Args required [FILE]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local FILE=$1
    if [ ! -f "$FILE" ]; then
        err "File [$FILE] not found."
        err "Number of args sent [$#]. Args [$@]"
        die $ERR_CFG_FILE_NOTFOUND
    fi
}

key_valid_else_die() # ARG1 [KEY]
{
    if [ $# != 1 ]; then
        err "Args required [KEY]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi
    local KEY=$1
    dbg "KEY [$KEY]"
}

val_valid_else_die() # ARG1 [VALUE]
{
    if [ $# != 1 ]; then
        err "Args required [VALUE]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi
    local VALUE=$1
    dbg "VALUE [$VALUE]"
}

set_key_value() # ARG1 [KEY] ARG2 [VALUE] ARG3 [FILE]
{
    if [ $# != 3 ]; then
        err "Args required key, value & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local VALUE=$2
    local CFG_FILE=$3

    dbg "Updating KEY [$KEY]; VALUE [$VALUE] CFG_FILE [$CFG_FILE]"

    key_valid_else_die $KEY
    val_valid_else_die "$VALUE"
    file_exists_else_die $CFG_FILE

    local TMP_DST_FILE="$CFG_FILE.`date '+%d%m%Y%H%M%s%S'`"
    local CFG_LINE_TO_WRITE=""
    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do

        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            dbg "Key [$KEY] in [$CFG_LINE] in founnd file [$CFG_FILE]."
            KEY_FOUND=$YES
            CFG_LINE_TO_WRITE="$KEY=$VALUE"
        else
            dbg "NO_MATCH CFG_LINE [$CFG_LINE]"
            CFG_LINE_TO_WRITE="$CFG_LINE"
        fi
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    done

    if [ $KEY_FOUND == $NO ]; then
        dbg "Key [$KEY] not found in file [$CFG_FILE]"
        dbg "Key [$KEY] will be added and set to Value [$VALUE]."
        CFG_LINE_TO_WRITE="$KEY=$VALUE"
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    fi
    unset $IFS

    cat $TMP_DST_FILE > $CFG_FILE
    rm -f $TMP_DST_FILE

    return $ERR_SUCCESS

}

do_m0provision_action()
{
    local CFG_LINE=""

    msg "Configuring host [`hostname -f`]"

    SALT_OPT=$(salt-call --local grains.get virtual)

    ANY_ERR=$(echo $SALT_OPT | grep -i ERROR | wc -l)
    if [ "$ANY_ERR" != "0" ]; then
        err "Salt command failed."
        msg "[$SALT_OPT]"
        die $ERR_CFG_SALT_FAILED
    fi

    CLUSTER_TYPE=$(echo $SALT_OPT | grep physical | wc -l)
    if [ "$CLUSTER_TYPE" != "1" ]; then
        msg "CLUSTER_TYPE is [$CLUSTER_TYPE]. Config operation is not allowed."
        msg "Only physical clusters will be configured here. "
        msg "[$SALT_OPT]"
        die $ERR_NOT_IMPLEMENTED
    fi

    dbg "[$SALT_OPT]"

    while IFS= read -r CFG_LINE
    do
        if [[ "$CFG_LINE" == "#"* ]]; then
            dbg "Commented line [$CFG_LINE]"
        else
            dbg "Not a commented line [$CFG_LINE]"
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); unset IFS;
            local KEY=${CFG_LINE_ARRAY[0]}
            local VALUE=${CFG_LINE_ARRAY[1]}
            KEY=$(echo $KEY | xargs)
            VALUE=$(echo $VALUE | xargs)
            if [ "$KEY" != "" ]; then
                msg "Updating KEY [$KEY]; VALUE [$VALUE]"
                set_key_value $KEY "$VALUE" $ETC_SYSCONFIG_MERO
            else
                dbg "Not processing [$CFG_LINE]"
            fi
        fi
    done < $MERO_CONF_FILE
    die $ERR_SUCCESS
}

get_option() # ARG1 [KEY] ARG2 [FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); IFS=$'\n'
            local K1=${CFG_LINE_ARRAY[0]}
            local V1=${CFG_LINE_ARRAY[1]}
            K1=$(echo $K1 | xargs)
            V1=$(echo $V1 | xargs)
            if [[ "$CFG_LINE" == "#"* ]]; then
                dbg "Key found in disabled CFG_LINE [$CFG_LINE]"
                msg "Disabled key [$KEY] has value [$V1]."
            else
                dbg "Key found in CFG_LINE [$CFG_LINE]"
                msg "Key [$K1] has value [$V1]."
                die $ERR_SUCCESS
            fi
        fi
    done
    unset IFS

    die $ERR_CFG_KEY_NOTFOUND
}

dsb_option() # ARG1 [KEY] ARG2 [FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    local TMP_DST_FILE="$CFG_FILE.`date '+%d%m%Y%H%M%s%S'`"
    local CFG_LINE_TO_WRITE=""
    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            if [[ "$CFG_LINE" == "#"* ]]; then
                msg "Already disabled [$CFG_LINE]"
                rm -f $TMP_DST_FILE
                die $ERR_CFG_KEY_OPNOTALLOWED
            else
                CFG_LINE_TO_WRITE="# $CFG_LINE"
                KEY_FOUND=$YES
            fi
        else
            CFG_LINE_TO_WRITE="$CFG_LINE"
        fi
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    done

    unset $IFS

    if [ $KEY_FOUND == $YES ]; then
        cat $TMP_DST_FILE > $CFG_FILE
        rm -f $TMP_DST_FILE
        die $ERR_SUCCESS
    fi
    rm -f $TMP_DST_FILE
    die $ERR_CFG_KEY_NOTFOUND
}

enb_option() # ARG1 [KEY] ARG2 [CFG_FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); IFS=$'\n'
            local K1=${CFG_LINE_ARRAY[0]}
            local V1=${CFG_LINE_ARRAY[1]}
            K1=$(echo $K1 | xargs)
            V1=$(echo $V1 | xargs)
            if [[ "$CFG_LINE" == "#"* ]]; then
                dbg "Key found in disabled CFG_LINE [$CFG_LINE]"
                msg "Disabled key [$K1] has value [$V1]."
                set_key_value $KEY "$V1" $CFG_FILE
                die $ERR_SUCCESS
            else
                dbg "Key found in CFG_LINE [$CFG_LINE]"
                msg "Key [$K1] has value [$V1] is already enabled."
                die $ERR_CFG_KEY_OPNOTALLOWED
            fi
        fi
    done
    unset IFS

    err "Key [$KEY] not found."
    die $ERR_CFG_KEY_NOTFOUND
}

set_option() # ARG1 [KEY] ARG2 [VALUE] ARG3 [CFG_FILE]
{
    if [ $# != 3 ]; then
        err "Args required key, value & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local VALUE=$2
    local CFG_FILE=$3

    key_valid_else_die $KEY
    val_valid_else_die "$VALUE"
    file_exists_else_die $CFG_FILE

    dbg "Updating KEY [$KEY]; VALUE [$VALUE] CFG_FILE [$CFG_FILE]"
    set_key_value $KEY "$VALUE" $CFG_FILE
    die $ERR_SUCCESS
}

## main
is_sudoed

workflow

if [ "$1" == "-e" ]; then
    enb_option "$2" "$3"
elif [ "$1" == "-d" ]; then
    dsb_option "$2" "$3"
elif [ "$1" == "-g" ]; then
    get_option "$2" "$3"
elif [ "$1" == "-s" ]; then
    set_option "$2" "$3" "$4"
else
    if [ $# == 0 ]; then
        PCMD=$(ps -o args= $PPID)
        dbg "PCMD [$PCMD]"
        if [[ "$PCMD" == *"m0provision config" ]]; then
            do_m0provision_action
        else
            err "The m0provision may only call this script without any args."
            err "PCMD [$PCMD]"
        fi
    fi
fi

err "Invalid args. Count [$#] Args [$@]"
help
die $ERR_INVALID_ARGS
