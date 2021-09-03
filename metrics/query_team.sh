#! /bin/bash

members=`for i in 1 2 3 4; do curl -s -u "johnbent:$GH_OATH" -X GET "https://api.github.com/orgs/Seagate/teams/cortx-global/members?per_page=100&page=$i" | grep login; done | awk '{print $2}' | tr -d '"' | tr -d ','`
pmembers=`for i in 1 2 3 4; do curl -s -u "johnbent:$GH_OATH" -X GET "https://api.github.com/orgs/Seagate/teams/cortx-pilot/members?per_page=100&page=$i" | grep login; done | awk '{print $2}' | tr -d '"' | tr -d ','`
for member in $members
do
  ./cortx_people.py -i $member
  ret=$?

  # if they aren't a known member, then check to see if they are in the pilot team
  if [ $ret -ne 0 ]; then
    echo $pmembers | grep -q $member
    ret=$?
    echo "$member is in the pilot team? $ret =?= 0"
  fi
done
