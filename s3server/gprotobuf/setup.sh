#!/bin/sh -x

rm -rf protobuf

git clone -b v3.0.0-alpha-3.1 https://github.com/google/protobuf.git

cd protobuf
INSTALL_DIR=/opt/seagate/s3/gprotobuf
mkdir $INSTALL_DIR

./autogen.sh
./configure --prefix=$INSTALL_DIR

make
make check
sudo make install
