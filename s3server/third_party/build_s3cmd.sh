#!/bin/sh -xe
# Script to build s3cmd.
# git repo: https://github.com/s3tools/s3cmd.git
# branch: previous develop commit: 4c2489361d353db1a1815172a6143c8f5a2d1c40 (1.6.1)
# Current develop commit: 4801552f441cf12dec53099a6abc2b8aa36ccca4 (2.0.2)

cd s3cmd

# Apply s3cmd patch
#patch -f -p1 < ../../patches/s3cmd.patch
patch -f -p1 < ../../patches/s3cmd_2.0.2_max_retries.patch

cd ..
