#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=3.0

cd ~/rpmbuild/SOURCES/
rm -rf ossperf

mkdir ossperf-${VERSION}

git clone https://github.com/christianbaun/ossperf  ossperf-${VERSION}
cd ossperf-${VERSION}
git checkout 58eafade5ada0f98d7b34f2d41cfc673c8d7b301
cd ..
tar -zcvf ossperf-${VERSION}.tar.gz ossperf-${VERSION}
rm -rf ossperf-${VERSION}
cp ${BASEDIR}/ossperf.patch .

cd ~/rpmbuild/SOURCES/

rpmbuild -ba ${BASEDIR}/ossperf.spec
