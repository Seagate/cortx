#!/bin/sh
#Script to run UT's of s3background delete
set -e

abort()
{
    echo >&2 '
***************
*** FAILED ***
***************
'
    echo "Error encountered. Exiting unit test runs prematurely..." >&2
    trap : 0
    exit 1
}
trap 'abort' 0

printf "\nRunning s3background delete UT's...\n"

SCRIPT_PATH=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$SCRIPT_PATH")

#Update python path to source modules and run unit tests.

PYTHONPATH=${PYTHONPATH}:${SCRIPT_DIR}/.. python36 -m pytest ${SCRIPT_DIR}/../ut/*.py

echo "s3background UT's runs successfully"

trap : 0

