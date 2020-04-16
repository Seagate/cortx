#!/bin/sh -x

rm -rf cpp-driver

git clone --branch 2.1.0 --depth=1 https://github.com/datastax/cpp-driver.git
cd cpp-driver

INSTALL_DIR=`pwd`/s3_dist/usr/

mkdir -p $INSTALL_DIR
mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH=$INSTALL_DIR ..

make
make install
cd ..

# make necessary adjustments for packaging.
fpm -s dir -t rpm -C s3_dist/ --name libcassandra --version 2.1.0 --description "DataStax C/C++ Driver for Apache Cassandra"

cd ..
