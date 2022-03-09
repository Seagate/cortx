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
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: Backlog", "color":"DFFF00"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: Fix Scheduled", "color":"FFBF00"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: L1 Triage", "color":"FF7F50"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: L2 Triage", "color":"DE3163"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: More Info Needed", "color":"9FE2BF"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: Resolved", "color":"40E0D0"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: Will NOT Fix", "color":"6495ED"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: DevAd", "color":"CCCCFF"}'
  curl -s -u "$GH_USER:$GH_OATH" -X POST "https://api.github.com/repos/$repo/labels" -d '{"name":"Status: DevTeam", "color":"0E8A16"}'
done
