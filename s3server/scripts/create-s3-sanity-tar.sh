#!/bin/sh

####################################
# Script to generate s3-sanity tar #
####################################

set -e

USAGE="USAGE: bash $(basename "$0") [-h | --help]
Generate tar file which has all required dependencies to run S3 sanity test.
where:
    --help      display this help and exit

Generated tar will have follwoing files:
    s3-sanity-test
    ├── jcloud
    │   └── jcloudclient.jar
    ├── ldap
    │   ├── add_test_account.sh
    │   ├── delete_test_account.sh
    │   └── test_data.ldif
    ├── README
    └── s3-sanity-test.sh"


case "$1" in
    --help | -h )
        echo "$USAGE"
        exit 0
        ;;
esac

TARGET_DIR="s3-sanity-test"
SRC_ROOT="$(dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) " )"

mkdir -p $TARGET_DIR/jcloud
mkdir -p $TARGET_DIR/ldap
cp $SRC_ROOT/auth-utils/jcloudclient/target/jcloudclient.jar $TARGET_DIR/jcloud/
cp $SRC_ROOT/scripts/ldap/add_test_account.sh $TARGET_DIR/ldap/
cp $SRC_ROOT/scripts/ldap/delete_test_account.sh $TARGET_DIR/ldap/
cp $SRC_ROOT/scripts/ldap/test_data.ldif $TARGET_DIR/ldap/
cp $SRC_ROOT/scripts/s3-sanity/s3-sanity-test.sh $TARGET_DIR/
cp $SRC_ROOT/scripts/s3-sanity/README $TARGET_DIR/

# create tar file
tar -cf s3-sanity-test.tar $TARGET_DIR
echo "$TARGET_DIR.tar created successfully."

# remove dir
rm -rf $TARGET_DIR
