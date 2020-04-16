#!/bin/bash

# cloud account params
source ./account.config
source ./dm_ip.list

HOSTS="$HOST,$REMOTE_HOSTS"
PDSH_PRIMARY_ACC_HOST="pdsh -w $HOST"
PDSH_TEST_HOSTS="pdsh -w $HOSTS"
PDCP_TEST_HOSTS="pdcp -w $HOSTS"
RPDCP_TEST_HOSTS="rpdcp -w $HOSTS"
TEST_DATA_DIR="/tmp/tct-sanity-test"
SRC_TEST_SCRIPT="./tct-single-node-sanity-test.sh"
TEST_SCRIPT="$TEST_DATA_DIR/sanity-test.sh"
TCT_STDOUT_LOG_DIR="tct-logs"
TCT_LOGS="$TCT_STDOUT_LOG_DIR/tct-sanity-runner.log"

mkdir $TCT_STDOUT_LOG_DIR > /dev/null 2>&1
touch $TCT_LOGS
echo -e "*** TCT Sanity Runner ***" > $TCT_LOGS
$PDSH_TEST_HOSTS "mkdir $TEST_DATA_DIR" >> $TCT_LOGS 2>&1
$PDCP_TEST_HOSTS $SRC_TEST_SCRIPT $TEST_SCRIPT >> $TCT_LOGS 2>&1
$PDSH_TEST_HOSTS "echo $SECRET_KEY > $PWD_FILE" >> $TCT_LOGS 2>&1

# start cloud gateway service on all data mover nodes
echo -e "Starting cloud gateway service..."
$PDSH_TEST_HOSTS "mmcloudgateway service start" >> $TCT_LOGS 2>&1

# create mmcloudgateway account command from primary node
create_account_cmd="mmcloudgateway account create --cloud-nodeclass $CLOUD_NODECLASS \
    --cloud-name $CLOUD_NAME --cloud-type $CLOUD_TYPE --username $USERNAME --pwd-file \
    $PWD_FILE --enable $ENABLE --etag-enable $ETAG_ENABLE"
[[ -z $CLOUD_URL ]] || create_account_cmd="$create_account_cmd --cloud-url $CLOUD_URL"
[[ -z $SERVER_CERT_PATH ]] || create_account_cmd="$create_account_cmd --server-cert-path $SERVER_CERT_PATH"
echo -e "Creating cloud account..."
$PDSH_PRIMARY_ACC_HOST $create_account_cmd >> $TCT_LOGS 2>&1

# delete secret key file
$PDSH_PRIMARY_ACC_HOST "rm -f $PWD_FILE" >> $TCT_LOGS 2>&1

# start tct sanity tests on all tct gateway nodes
echo -e "Starting TCT sanity tests..."
echo -e "\nRESULT:"
$PDSH_TEST_HOSTS "bash $TEST_SCRIPT $FS_ROOT"

# delete mmcloudgateway account from primary node
echo -e "\nDeleting cloud account..."
$PDSH_PRIMARY_ACC_HOST "mmcloudgateway account delete --cloud-nodeclass $CLOUD_NODECLASS \
    --cloud-name $CLOUD_NAME" >> $TCT_LOGS 2>&1

# collect stdout logs from all tct nodes
$RPDCP_TEST_HOSTS "$TEST_DATA_DIR/tct_stdout" $TCT_STDOUT_LOG_DIR

# remove test data dir
$PDSH_TEST_HOSTS "rm -rf $TEST_DATA_DIR" >> $TCT_LOGS 2>&1
