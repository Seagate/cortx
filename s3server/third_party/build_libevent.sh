#!/bin/sh -xe
# Script to build libevent.
# git repo: https://github.com/libevent/libevent.git
# gerrit repo: git clone http://gerrit.mero.colo.seagate.com:8080/libevent
# previous tag: release-2.0.22-stable commit: c51b159cff9f5e86696f5b9a4c6f517276056258
# previous patch: libevent-release-2.0.22-stable.patch
# Current tag: release-2.1.10-stable commit: 64a25bcdf54a3fc8001e76ea51e0506320567e17

cd libevent

#previous libevent patch was libevent-release-2.0.22-stable.patch

# Apply the current libevent patch for memory pool support
patch -f -p1 < ../../patches/libevent-release-2.1.10-stable.patch

INSTALL_DIR=`pwd`/s3_dist
rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

./autogen.sh
./configure --prefix=$INSTALL_DIR

make clean
make
#make verify
make install

cd ..
