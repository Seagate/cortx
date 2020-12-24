#! /usr/bin/env python3

import cortx_community 

import argparse
import dateutil.parser as dateparser
import datetime
import json
import os
import pickle
import pprint
import sys
import time
from github import Github

def Debug(msg):
  print(msg)

def main():
  parser = argparse.ArgumentParser(description='Retrieve all activity done by a particular user.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('login', metavar='LOGIN', type=str, help="The login of the person for whom you wish to see their activity")
  parser.add_argument('-s', '--since', type=str, help="Only show activity since yyyy-mm-dd")
  parser.add_argument('-u', '--until', type=str, help="Only show activity until yyyy-mm-dd")
  parser.add_argument('-l', '--last-week', action='store_true', help="Only show activity in the last seven days")
  args = parser.parse_args()

  activity = cortx_community.CortxActivity()

  if args.since:
    args.since = dateparser.parse(args.since)
  if args.until:
    args.until = dateparser.parse(args.until)
  print(args.since, args.until)

  activities = {}
  try:
    # create a new structure to hold the data in an organization that is more easily sorted
    # go through the activity and save each into the new format
    # problem is that the watch event doesn't have a date for it . . . 
    for (url,created_at) in activity.get_activity(args.login):
      if created_at is None:
        created_at = datetime.datetime.today()
      activities[created_at] = url
  except KeyError: 
    print("Login %s has no observed activity" % args.login)
    sys.exit(0)

  # using the new data structure, print the info
  for d,u in sorted(activities.items()):
    if args.since and d < args.since:
      continue
    if args.until and d > args.until:
      continue
    print("%s %s %s" % (args.login,d,u))

if __name__ == "__main__":
    main()

