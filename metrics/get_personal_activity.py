#! /usr/bin/env python3

import cortx_community 

import argparse
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
  args = parser.parse_args()

  activity = cortx_community.CortxActivity()

  try:
    # create a new structure to hold the data in an organization that is more easily sorted
    # go through the activity and save each into the new format
    activities = {}
    for (url,created_at) in activity.get_activity(args.login):
      activities[created_at] = url
  except KeyError: 
    print("Login %s has no observed activity" % args.login)
    sys.exit(0)

  # using the new data structure, print the info
  for d,u in sorted(activities.items()):
    print("%s %s %s" % (args.login,d,u))

if __name__ == "__main__":
    main()

