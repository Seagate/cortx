#!/bin/sh

####################################
# Script to generate TCT-sanity tar #
####################################

set -e

USAGE="USAGE: bash $(basename "$0") [--help]
Generate tar file which has all required dependencies to run TCT sanity test.
where:
    --help      display this help and exit

Generated tar will have following files
 tct-sanity-test
    ├── account.config
    ├── dm_ip.list
    ├── README
    ├── runner.sh
    └── tct-single-node-sanity-test.sh"

case "$1" in
    --help )
        echo "$USAGE"
        exit 0
        ;;
esac

TARGET_DIR="tct-sanity-test"
SRC_ROOT="$(dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) " )"

mkdir -p $TARGET_DIR
cp $SRC_ROOT/scripts/tct-sanity/account.config $TARGET_DIR/
cp $SRC_ROOT/scripts/tct-sanity/dm_ip.list $TARGET_DIR/
cp $SRC_ROOT/scripts/tct-sanity/README $TARGET_DIR/
cp $SRC_ROOT/scripts/tct-sanity/runner.sh $TARGET_DIR/
cp $SRC_ROOT/scripts/tct-sanity/tct-single-node-sanity-test.sh $TARGET_DIR/

# create tar file
tar -cf tct-sanity-test.tar $TARGET_DIR
echo "$TARGET_DIR.tar created successfully."

# remove dir
rm -rf $TARGET_DIR
