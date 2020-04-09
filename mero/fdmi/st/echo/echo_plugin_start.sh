#!/bin/sh

#set -x

ECHO_DIR=`dirname $0`
SRC_DIR=$ECHO_DIR/../../..

$SRC_DIR/m0t1fs/linux_kernel/st/st insmod
$ECHO_DIR/m0fdmiecho
$SRC_DIR/m0t1fs/linux_kernel/st/st rmmod
