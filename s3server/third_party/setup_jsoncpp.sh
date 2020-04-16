#!/bin/sh -xe
# Script to setup jsoncpp.
# git repo: https://github.com/open-source-parsers/jsoncpp.git
# tag: 1.6.5 commit: d84702c9036505d0906b2f3d222ce5498ec65bc6

cd jsoncpp

# Generate amalgamated source for inclusion in project.
# https://github.com/open-source-parsers/jsoncpp#using-jsoncpp-in-your-project
python amalgamate.py

cd ..
