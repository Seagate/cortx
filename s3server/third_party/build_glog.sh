#!/bin/sh -xe
# Script to build glog.
# git repo: https://github.com/google/glog.git
# tag: v0.3.4 commit: d8cb47f77d1c31779f3ff890e1a5748483778d6a

cd glog

INSTALL_DIR=`pwd`/s3_dist
rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

GFLAGS_HDR_DIR=../gflags/s3_dist/include
GFLAGS_LIB_DIR=../gflags/s3_dist/lib

CXXFLAGS="-fPIC -I$GFLAGS_HDR_DIR" LDFLAGS="-L$GFLAGS_LIB_DIR" ./configure --prefix=$INSTALL_DIR

touch configure.ac aclocal.m4 configure Makefile.am Makefile.in
make clean
make
make install

cd ..
