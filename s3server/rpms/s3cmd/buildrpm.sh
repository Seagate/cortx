#!/bin/sh

set -xe
OS_VERSION=$(cat /etc/os-release | grep -w VERSION_ID | cut -d '=' -f 2)
SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")
# previously version was 1.6.1, commit version was 4c2489361d353db1a1815172a6143c8f5a2d1c40
VERSION=2.0.2
COMMIT_VER=4801552f441cf12dec53099a6abc2b8aa36ccca4
SHORT_COMMIT_VER=4801552

cd ~/rpmbuild/SOURCES/
rm -rf s3cmd*

git clone -b v${VERSION} http://github.com/s3tools/s3cmd s3cmd-${VERSION}-${SHORT_COMMIT_VER}
cd s3cmd-${VERSION}-${SHORT_COMMIT_VER}
git checkout 4801552f441cf12dec53099a6abc2b8aa36ccca4
cd ~/rpmbuild/SOURCES/
tar -zcvf s3cmd-${VERSION}-${SHORT_COMMIT_VER}.tar.gz s3cmd-${VERSION}-${SHORT_COMMIT_VER}
rm -rf s3cmd-${VERSION}-${SHORT_COMMIT_VER}

cp ${BASEDIR}/s3cmd_${VERSION}_max_retries.patch .

if [ "$OS_VERSION" = "\"8.0\"" ]; then
  rpmbuild -ba ${BASEDIR}/s3cmd.spec --define 's3_with_python36_ver8 1'
else
  rpmbuild -ba ${BASEDIR}/s3cmd.spec --define 's3_with_python36 1'
fi

