#!/bin/sh -xe
# Script to build gflags.
# git repo: https://github.com/gflags/gflags.git
# tag: v2.2.0 commit: f8a0efe03aa69b3336d8e228b37d4ccb17324b88

cd gflags

INSTALL_DIR=`pwd`/s3_dist
rm -rf build/ $INSTALL_DIR
mkdir $INSTALL_DIR
mkdir build && cd build

CFLAGS=-fPIC CXXFLAGS=-fPIC cmake .. -DCMAKE_INSTALL_PREFIX:PATH=$INSTALL_DIR
make
make install

cd ../..
