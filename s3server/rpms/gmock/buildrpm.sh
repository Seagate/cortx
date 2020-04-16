#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=1.7.0

cd ~/rpmbuild/SOURCES/
rm -rf gmock* googlemock*

git clone -b release-${VERSION} http://gerrit.mero.colo.seagate.com:8080/googlemock gmock-${VERSION}
git clone -b release-${VERSION} http://gerrit.mero.colo.seagate.com:8080/googletest gmock-${VERSION}/gtest
tar -zcvf gmock-${VERSION}.tar.gz gmock-${VERSION}
rm -rf gmock-${VERSION} googlemock

cd -

rpmbuild -ba ${BASEDIR}/gmock.spec
