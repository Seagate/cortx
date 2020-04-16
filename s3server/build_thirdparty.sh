#!/bin/sh
# Script to build third party libs
set -xe

usage() {
  echo 'Usage: ./build_thirdparty.sh [--no-mero-build][--help]'
  echo 'Optional params as below:'
  echo '          --no-mero-build  : If this option is set, then do not build mero.'
  echo '                             Default is (false). true = skip mero build.'
  echo '          --help (-h)      : Display help'
}

# read the options
OPTS=`getopt -o h --long no-mero-build,help -n 'build_thirdparty.sh' -- "$@"`

eval set -- "$OPTS"

no_mero_build=0
# extract options and their arguments into variables.
while true; do
  case "$1" in
    --no-mero-build) no_mero_build=1; shift ;;
    -h|--help) usage; exit 0;;
    --) shift; break ;;
    *) echo "Internal error!" ; exit 1 ;;
  esac
done

# Always refresh to ensure thirdparty patches can be applied.
./refresh_thirdparty.sh

# Before we build s3, get all dependencies built.
S3_SRC_FOLDER=`pwd`
cd third_party
./build_libevent.sh
./build_libevhtp.sh
./setup_jsoncpp.sh

if [ $no_mero_build -eq 0 ]
then
  ./build_mero.sh
fi

cd $S3_SRC_FOLDER
