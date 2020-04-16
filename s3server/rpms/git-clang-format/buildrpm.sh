#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=6.0
GIT_RELEASE_BRANCH=release_60

cd ~/rpmbuild/SOURCES/
rm -rf git-clang-format*

mkdir git-clang-format-${VERSION}

wget -O git-clang-format-${VERSION}/git-clang-format https://raw.githubusercontent.com/llvm-mirror/clang/${GIT_RELEASE_BRANCH}/tools/clang-format/git-clang-format
wget -O git-clang-format-${VERSION}/LICENSE.TXT https://raw.githubusercontent.com/llvm-mirror/clang/${GIT_RELEASE_BRANCH}/LICENSE.TXT

tar -zcvf git-clang-format-${VERSION}.tar.gz git-clang-format-${VERSION}
rm -rf git-clang-format-${VERSION}

cd -

rpmbuild -ba ${BASEDIR}/git-clang-format.spec
