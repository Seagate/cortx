#! /bin/bash

# ugh can't figure out how to use the variable
# have to hard code in the below curl line
LABEL="Status: Confirm Fix"
COLOR="0E8A16"

PYTHON=$(cat <<-END
import cortx_community as cc
repos = cc.get_repos()
for r in repos:
  print(r.full_name)
END
)

echo $PYTHON

for repo in `python3 -c "$PYTHON"`
do
  echo $repo
  curl -s -u "johnbent:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: Confirm Fix", "color":"0E8A16"}'
done
