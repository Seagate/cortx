#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

VERSION=1.7.0

cd ~/rpmbuild/SOURCES/
rm -rf bazel*

wget -O bazel-0.13.0.zip https://github.com/bazelbuild/bazel/releases/download/0.13.0/bazel-0.13.0-dist.zip

cd -

rpmbuild -ba ${BASEDIR}/bazel.spec
