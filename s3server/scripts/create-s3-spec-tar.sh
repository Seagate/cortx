#!/bin/sh
# Script to generate tar containing S3 specs (S3 system tests scripts)
set -e

TAR_NAME=s3-specs
TARGET_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/$TAR_NAME"
SRC_ROOT="$(dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) " )"
TEST_DIR="$TARGET_DIR/st/clitests"

mkdir -p $TEST_DIR/cfg
mkdir -p $TEST_DIR/resources
mkdir -p $TEST_DIR/test_data

working_dir=$(pwd)

# copy jclient.jar
cp $SRC_ROOT/auth-utils/jclient/target/jclient.jar $TEST_DIR

# copy jcloudclient.jar
cp $SRC_ROOT/auth-utils/jcloudclient/target/jcloudclient.jar $TEST_DIR


# copy test framework and spec files
cd $SRC_ROOT/st/clitests
test_files=$(git ls-files)
for file in $test_files
do
    cp -f $file $TEST_DIR/$file
done

cd $working_dir
# create tar file
tar -cf s3-specs.tar $TAR_NAME

# remove dir
rm -rf $TARGET_DIR
