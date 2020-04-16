#!/bin/sh -x

rm -rf jsoncpp

git clone -b 1.6.5 https://github.com/open-source-parsers/jsoncpp.git

cd jsoncpp

# Generate amalgamated source for inclusion in project.
# https://github.com/open-source-parsers/jsoncpp#using-jsoncpp-in-your-project
python amalgamate.py
