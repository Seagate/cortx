#!/usr/bin/env bash

CWD=$(cd "$( dirname "$0")" && pwd)

source $CWD/st-config.sh
TEST_TYPE="ping"
MSG_NR=1048576
MSG_SIZE=4k
CONCURRENCY_CLIENT=8
CONCURRENCY_SERVER=16

source $CWD/run-1x1.sh
