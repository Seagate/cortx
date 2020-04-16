#!/bin/sh -xe
# Script to build libxml2.
# git repo: https://github.com/GNOME/libxml2.git
# tag: v2.9.2 commit: 726f67e2f140f8d936dfe993bf9ded3180d750d2

cd libxml2

INSTALL_DIR=`pwd`/s3_dist
rm -rf $INSTALL_DIR
mkdir $INSTALL_DIR

./autogen.sh
./configure --prefix=$INSTALL_DIR --without-python --without-icu

make clean
make
#make verify
make install

cd ..
