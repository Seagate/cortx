#!/bin/bash

FS_ROOT=$1
TCT_TEST_DATA_DIR=$(mktemp -d --tmpdir=$FS_ROOT)
TEST_FILE="$TCT_TEST_DATA_DIR/48MB-data.$(hostname)"
TCT_CONSOLE_LOG="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/tct_stdout"

function cleanup {
    rm -rf $TCT_TEST_DATA_DIR
}
trap cleanup EXIT

touch $TCT_CONSOLE_LOG
echo "*** TCT Sanity test stout: $(hostname) ***" > $TCT_CONSOLE_LOG
echo "Create file: $TEST_FILE" >> $TCT_CONSOLE_LOG
dd if=/dev/urandom of=$TEST_FILE bs=16M count=3 status=none

TCT_TEST_RESULT=""


BEFORE_MD5=$(md5sum $TEST_FILE)
mmcloudgateway files migrate $TEST_FILE >> $TCT_CONSOLE_LOG 2>&1

mmcloudgateway files list $TEST_FILE 2>> $TCT_CONSOLE_LOG | grep -q 'Non-resident' &&
    TCT_TEST_RESULT="MIGRATION: OK" || TCT_TEST_RESULT="MIGRATION: FAILED"

mmcloudgateway files recall $TEST_FILE >> $TCT_CONSOLE_LOG 2>&1

mmcloudgateway files list $TEST_FILE 2>> $TCT_CONSOLE_LOG | grep -q 'Co-resident' &&
    TCT_TEST_RESULT="$TCT_TEST_RESULT\tRECALL: OK" ||
    TCT_TEST_RESULT="$TCT_TEST_RESULT\tRECALL: FAILED"

AFTER_MD5=$(md5sum $TEST_FILE)

[[ $BEFORE_MD5 == $AFTER_MD5 ]] &&
    TCT_TEST_RESULT="$TCT_TEST_RESULT\tDATA INTEGRITY: OK" ||
    TCT_TEST_RESULT="$TCT_TEST_RESULT\tDATA INTEGRITY: FAILED"

mmcloudgateway files delete --delete-local-file $TEST_FILE >> $TCT_CONSOLE_LOG 2>&1

mmcloudgateway files reconcile $FS_ROOT >> $TCT_CONSOLE_LOG 2>&1

echo -e "\n$TCT_TEST_RESULT" >> $TCT_CONSOLE_LOG
echo -e $TCT_TEST_RESULT
