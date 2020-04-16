#!/bin/sh
set -x
git clone http://gerrit.mero.colo.seagate.com/mero
./mero/scripts/install-build-deps
rm -rf mero
