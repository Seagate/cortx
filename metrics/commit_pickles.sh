#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

git pull
git add pickles/*
git commit -m "Updating the pickles containing the cortx community tracking metrics"
git push
