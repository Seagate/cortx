#!/bin/sh -xe
# Script to build gtest.
# git repo: https://github.com/google/googletest.git
# tag: release-1.7.0 commit: c99458533a9b4c743ed51537e25989ea55944908

cd googletest

rm -rf build/
mkdir build

cd build
CFLAGS=-fPIC CXXFLAGS=-fPIC cmake ..
make

cd ..
cd ..
