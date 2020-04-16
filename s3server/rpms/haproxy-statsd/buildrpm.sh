#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=1.0

cd ~/rpmbuild/SOURCES/
rm -rf ossperf

mkdir haproxy-statsd-${VERSION}

git clone https://github.com/softlayer/haproxy-statsd  haproxy-statsd-${VERSION}
tar -zcvf haproxy-statsd-${VERSION}.tar.gz haproxy-statsd-${VERSION}
rm -rf haproxy-statsd-${VERSION}
cp ${BASEDIR}/haproxy-statsd.patch .

cd -

rpmbuild -ba ${BASEDIR}/haproxy-statsd.spec
