#! /bin/bash

# ugh can't figure out how to use the variable
# have to hard code in the below curl line
#LABEL="SODACODE2022"
#COLOR="37E1E6"

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
  curl -s -u "johnbent:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"SODALOW", "color":"0E8A16"}'
done
