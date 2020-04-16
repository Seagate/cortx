#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=0.10.0

cd ~/rpmbuild/SOURCES/
rm -rf apache-log4cxx*

# Todo : log4cxx should be added to our gerrit repo to clone here
git clone https://gitbox.apache.org/repos/asf/logging-log4cxx.git apache-log4cxx-${VERSION}
tar -jcvf apache-log4cxx-${VERSION}.tar.bz2 apache-log4cxx-${VERSION}
rm -rf apache-log4cxx-${VERSION}

cp ${BASEDIR}/apache-log4cxx-${VERSION}.patch .

cd -

rpmbuild -ba ${BASEDIR}/apache-log4cxx.spec
