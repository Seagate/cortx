#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

git lfs prune # clean up space, I ran out of quote once due to not running this
git pull
git add pickles/*
git commit -m "Updating the pickles containing the cortx community tracking metrics"
git push
