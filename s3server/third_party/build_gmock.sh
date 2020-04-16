#!/bin/sh -xe
# Script to build gmock.
# git repo: https://github.com/google/googlemock.git
# tag: release-1.7.0 commit: c440c8fafc6f60301197720617ce64028e09c79d

cd googlemock

rm -f gtest
ln -s ../googletest gtest

rm -rf build/
mkdir build

cd build
CFLAGS=-fPIC CXXFLAGS=-fPIC cmake ..
make

cd ..
cd ..
