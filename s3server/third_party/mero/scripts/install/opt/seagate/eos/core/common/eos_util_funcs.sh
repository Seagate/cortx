#!/usr/bin/env bash

YES="YES"
NO="NO"
DEBUG=$NO

msg()
{
    local DI=""

    if [ $DEBUG == $YES ]; then
        DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "
    fi

    echo -e "\n$DI[MSG] $1"
}

err()
{
    local DI=""

    if [ $DEBUG == $YES ]; then
        DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "
    fi

    echo -e "\n$DI[ERR] $1"
}

die()
{
    local RESULT=$1
    local DI=""
    local CMSG=""

    if [ $DEBUG == $YES ]; then
        DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "
    fi

    if [ $RESULT == $ERR_NOT_IMPLEMENTED ]; then
        CMSG="RESULT ERR_NOT_IMPLEMENTED [$ERR_NOT_IMPLEMENTED] is treated as "
        CMSG=$CMSG"ERR_SUCCESS [$ERR_SUCCESS]."
        RESULT=$ERR_SUCCESS
    fi

    if [ $RESULT == $ERR_SUCCESS ]; then
        echo -e "\n$DI[MSG] $CMSG Terminating with RESULT [$RESULT]."
    else
        echo -e "\n$DI[ERR] Terminating with RESULT [$RESULT]."
    fi

    exit $RESULT
}

is_sudoed()
{
    local DI=""

    if [ $DEBUG == $YES ]; then
        DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "
    fi

    if [[ $EUID -ne 0 ]]; then
        echo -e "\n$DI[MSG] This program must be run with sudo."
        echo -e "\n$DI[MSG] sudo $0 [OPTIONS | ACTION] [ARGS  | PARAMETERS]"
        exit $ERR_SUDO_REQUIRED
    fi
}

die_if_failed()
{
    local RESULT=$1
    local ERR_MSG=$2
    local DI=""

    if [ $DEBUG == $YES ]; then
        DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "
    fi

    if [ $RESULT != 0 ]; then
        echo -e "\n$DI[ERR] RESULT [$RESULT] $ERR_MSG"
        exit $RESULT
    fi
}

dbg()
{
    local DI=""

    DI="[`basename ${BASH_SOURCE[1]}`::${FUNCNAME[1]}:${BASH_LINENO[0]}] "

    if [ $DEBUG == $YES ]; then
       echo -e "\n$DI[DBG] $1"
    fi
}
