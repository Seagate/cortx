#! /bin/bash

TEAM="cortx-developer-advocates"
PERM="triage" # ugh, can't figure out how this can be a variable, just edit manually in below for statement


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
  curl -s -u "$GH_USER:$GH_OATH" -X PUT "https://api.github.com/orgs/Seagate/teams/$TEAM/repos/$repo" -d '{"permission":"triage"}'
done
