#!/usr/bin/env bash

CWD=$(cd "$( dirname "$0")" && pwd)

source $CWD/st-config.sh
TEST_TYPE="bulk"
MSG_NR=1048576
MSG_SIZE=1m
CONCURRENCY_CLIENT=8
CONCURRENCY_SERVER=16
BD_BUF_NR_CLIENT=16
BD_BUF_NR_SERVER=32
BD_BUF_SIZE=4k
BD_BUF_NR_MAX=8

source $CWD/run-1x1.sh
