#!/bin/sh -x

rm -rf libuv

git clone -b v1.4.2 --depth=1 https://github.com/libuv/libuv.git
cd libuv

INSTALL_DIR=`pwd`/s3_dist/usr/
mkdir $INSTALL_DIR

./autogen.sh
./configure --prefix=$INSTALL_DIR

make
make check
make install

# make necessary adjustments for packaging.
mv s3_dist/usr/lib s3_dist/usr/lib64

sed -i -r "s/(^.*prefix=).*/\1\/usr/" s3_dist/usr/lib64/pkgconfig/libuv.pc
sed -i -r "s/(.*)lib$/\1lib64/" s3_dist/usr/lib64/pkgconfig/libuv.pc

fpm -s dir -t rpm -C s3_dist/ --name libuv --version 1.4.2 --description "Cross-platform asynchronous I/O"

cd ..
