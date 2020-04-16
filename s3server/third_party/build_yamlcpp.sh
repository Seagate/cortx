#!/bin/sh -xe
# Script to build yaml-cpp.
# git repo: https://github.com/jbeder/yaml-cpp.git
# branch: master commit: 7d2873ce9f2202ea21b6a8c5ecbc9fe38032c229
# Note:
# 0.5.3 uses boost, however master branch makes use of C++11,
# checking out above commit tag until 0.6.0 is released.
# https://github.com/jbeder/yaml-cpp/issues/264

cd yaml-cpp

INSTALL_DIR=`pwd`/s3_dist
rm -rf build/ $INSTALL_DIR
mkdir build
mkdir $INSTALL_DIR

cd build
CFLAGS=-fPIC CXXFLAGS=-fPIC cmake -DCMAKE_INSTALL_PREFIX:PATH=$INSTALL_DIR -DBUILD_SHARED_LIBS=ON ..
make
make install

cd ..
cd ..
