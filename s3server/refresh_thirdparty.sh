#!/bin/sh
# Script to refresh third party code base.
#   This script will undo any changes made in third party
#   submodules source code and will clone missing submodules.
set -xe

git submodule update --init --recursive --force
